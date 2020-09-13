import java.net.*;
import java.util.Arrays;
import java.nio.ByteBuffer;
import java.nio.ByteOrder;
import java.io.DataInputStream;


final DataType DISPLAY_DATA_TYPE = DataType.RGB; //Set to DataType.RGB to see RGB data or DataType.DEPTH to see depth data
final int SERVER_PORT = 6969;
final String SERVER_IP = "192.168.0.237";



static void error(Exception e) {
  e.printStackTrace();
  System.exit(1);
}

static void error(String e) {
  error(new Exception(e));
}


public enum DataType {
  DEPTH,
  RGB;
  
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
}


public enum Request {
  DISCONNECT,
  CONNECT;
  
  public String toString() {
    switch (this) {
    case CONNECT:
      return "c";
    case DISCONNECT:
      return "d";
    }
    error("Can't convert Request to Character");
    return null;
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
}


class TCPClient {
  private Socket socket;
  private DataInputStream socketReader;
 
  public TCPClient(String serverAddress, int serverPort) {
    try {
      this.socket = new Socket(serverAddress, serverPort);
      this.socketReader = new DataInputStream(this.socket.getInputStream()); // need to wrap in a DataInputStream so that we get the readFully() function (plain InputStream's read(n) function reads a MAXIMUM of n bytes, readFully always reads n bytes
    } catch(Exception e) { error(e); }
  }
  
  public Message receive() {
    byte[] header = receiveBytes(Message.HEADER_SIZE);
    DataType dataType = DataType.fromByte(header[0]);
    int dataLength = readBytesAsUInt32(header, 1, 4) - Message.HEADER_SIZE;
    byte[] data = receiveBytes(dataLength);
    return new Message(dataType, data);
  }
  
  public byte[] receiveBytes(int numBytes) {
    byte[] bytes = new byte[numBytes];
    try {
      socketReader.readFully(bytes);
    } catch (IOException e) { error(e); }
    return bytes;
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


TCPClient CLIENT;


void setup() {
  CLIENT = new TCPClient(SERVER_IP, SERVER_PORT);
  size(640, 480);
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
    if (data[b+1] == 7){
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
  Message frame = CLIENT.receive();
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
