import java.net.*;
import java.util.Arrays;
import java.nio.ByteBuffer;
import java.nio.ByteOrder;
import java.io.DataInputStream;


final DataType DISPLAY_DATA_TYPE = DataType.RGB; //Set to DataType.RGB to see RGB data or DataType.DEPTH to see depth data
final int SERVER_OUTPUT_PORT = 6969;
final int SERVER_INPUT_PORT = 7878;
final int DRIVE_SERVER_PORT = 55555;
final String SERVER_IP = "192.168.60.144";



static void error(Exception e) {
  e.printStackTrace();
  System.exit(1);
}

static void error(String e) {
  error(new Exception(e));
}


public enum DataType {
  DEPTH, RGB, DISCONNECT, TILT, STOP_SERVER, UNKNOWN;
  
  public static DataType fromByte(byte b) {
    switch (b) {
    case 'd': 
      return DataType.DEPTH;
    case 'r':
      return DataType.RGB;
    }
    error("Couldn't convert byte: " + b);
    return null;
  }
  
  public Byte toByte() {
    switch (this) {
      case DISCONNECT:
        return 'd';
      case TILT:
        return 't';
      case STOP_SERVER:
        return 's';
      case UNKNOWN:
        return 'u';
      }
    error("Can't convert Request to Character");
    return null; // unreachable
  }
}


class Message {
  public DataType dataType;
  public byte[] data;

  public final static int HEADER_SIZE = 5;
  
  public Message(DataType dataType, byte[] data) {
    this.dataType = dataType;
    this.data = data;
  }
  
  public byte[] serializeHeader() {
    ByteBuffer header = ByteBuffer.allocate(HEADER_SIZE).order(ByteOrder.LITTLE_ENDIAN);
    header.put(dataType.toByte());
    header.putInt(1, data.length + HEADER_SIZE);
    return header.array();
  }
}


class TCPClient {
  private Socket socket;
  private DataInputStream socketReader;
  private OutputStream socketWriter;
 
  public TCPClient(String serverAddress, int serverPort) {
    try {
      this.socket = new Socket(serverAddress, serverPort);
      this.socketReader = new DataInputStream(this.socket.getInputStream()); // need to wrap in a DataInputStream so that we get the readFully() function (plain InputStream's read(n) function reads a MAXIMUM of n bytes, readFully always reads n bytes
      this.socketWriter = this.socket.getOutputStream();
    } catch(Exception e) { error(e); }
  }
  
  public synchronized Message receive() {
    byte[] header = receiveBytes(Message.HEADER_SIZE);
    DataType dataType = DataType.fromByte(header[0]);
    int dataLength = readBytesAsUInt32(header, 1, 4) - Message.HEADER_SIZE;
    byte[] data = receiveBytes(dataLength);
    return new Message(dataType, data);
  }
  
  public synchronized byte[] receiveBytes(int numBytes) {
    byte[] bytes = new byte[numBytes];
    try {
      socketReader.readFully(bytes);
    } catch (IOException e) { error(e); }
    return bytes;
  }
  
  public synchronized void send(Message message) {
    sendBytes(message.serializeHeader());
    sendBytes(message.data);
  }
  
  public synchronized void sendBytes(byte[] bytes) {
    try {
      socketWriter.write(bytes);
    } catch (IOException e) { error(e); }
  }
  
  public synchronized void sendString(String data) {
    sendBytes(data.getBytes()); 
  }
 
  public void close() {
    try {
      socket.close();
    } catch (IOException e) { error(e); }
  }
  
  private int readBytesAsUInt32(byte[] array, int firstByte, int numBytes) {
    ByteBuffer bigEndianIntReader = ByteBuffer.wrap(array, firstByte, numBytes).order(ByteOrder.LITTLE_ENDIAN);
    long extractedLong = bigEndianIntReader.getInt() & 0xFFFFFFFFL; // HACK to get signed int from bytes, then get rid of sign by keeping the same bits but converting to a long (CAN'T DO `| 0F` BECAUSE IT GETS OPTIMIZED AWAY BY THE COMPILER)
    return (int)extractedLong;
  }
}


TCPClient RECEIVER;
TCPClient SENDER;
TCPClient DRIVE_CLIENT;


void setup() {
  RECEIVER = new TCPClient(SERVER_IP, SERVER_OUTPUT_PORT);
  SENDER = new TCPClient(SERVER_IP, SERVER_INPUT_PORT);
  DRIVE_CLIENT = new TCPClient(SERVER_IP, DRIVE_SERVER_PORT);
  thread("driveClientHeartbeat"); // start sending drive client heartbeat in a new thread
  size(640, 480);
}


void driveClientHeartbeat() {
  for (;;) {
    DRIVE_CLIENT.sendBytes("heartbeat".getBytes()); 
  }
}


void display_rgb(byte[] data) {
  for (int b = 0; b < data.length; b += 3) {
    color newColor = color(
      data[b] & 0xff, // SAME HACK AS BEFORE to convert bytes which are being interpreted as signed into ints
      data[b+1] & 0xff,
      data[b+2] & 0xff
    );
    int pixelIndex = b/3;
    pixels[pixelIndex] = newColor;
  }
}


void display_depth(byte[] data) {
  for (int b = 0; b < data.length; b += 2) {
    int colorIntensity;
    if (data[b+1] == 7) {
      colorIntensity = 0;
    } else {
      colorIntensity = ((data[b] & 0xff) + (data[b+1] << 8) ) >> 2;
    }
    color newColor = color(colorIntensity, colorIntensity, colorIntensity);
    int pixelIndex = b/2;
    pixels[pixelIndex] = newColor;
  }
}


void draw() {
  Message frame = RECEIVER.receive();
  if (frame.dataType == DISPLAY_DATA_TYPE) {
    loadPixels();
    if (frame.dataType == DataType.RGB ) {
      display_rgb(frame.data);
    } else if (frame.dataType == DataType.DEPTH ) {
      display_depth(frame.data);
    }
    updatePixels();
  }
}


int CURRENT_ANGLE = 0;
String DRIVE_KEYS = "wasd";

void keyPressed() {
  if (keyCode == UP || keyCode == DOWN) {
    
    int newAngle = CURRENT_ANGLE;
    switch (keyCode) {
      case UP:
        newAngle = min(CURRENT_ANGLE + 5, 30);
        break;
      case DOWN:
        newAngle = max(CURRENT_ANGLE - 5, -30);
        break;
    }
  
    if (newAngle != CURRENT_ANGLE) {
      print(newAngle);
      SENDER.send(new Message(DataType.TILT, new byte[]{(byte)newAngle}));
      CURRENT_ANGLE = newAngle;
    }
  }
  
  else if (key == 'q') {
    SENDER.send(new Message(DataType.STOP_SERVER, new byte[]{}));
  }
  
  else if (DRIVE_KEYS.indexOf(key) != -1) {
    String command = "";
    switch (key) {
      case 'w': // forward
        command = "70,70";
        break;
      case 's': // backward
        command = "-70,-70";
        break;
      case 'a': // left
        command = "-70,70";
        break;
      case 'd': // right
        command = "70,-70";
        break;
    }
    DRIVE_CLIENT.sendString(command); 
  }
}


void keyReleased() {
  if (DRIVE_KEYS.indexOf(key) != -1) {
    DRIVE_CLIENT.sendString("0,0");
  }
}
