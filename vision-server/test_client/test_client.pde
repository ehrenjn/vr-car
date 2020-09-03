import java.net.*;
import java.util.Arrays;
import java.nio.ByteBuffer;
import java.nio.ByteOrder;


final DataType DISPLAY_DATA_TYPE = DataType.RGB; //Set to DataType.RGB to see RGB data or DataType.DEPTH to see depth data
final int SERVER_PORT = 6969;
final String SERVER_IP = "192.168.0.237";
final int HEADER_SIZE = 12;
final int MAX_MESSAGE_LENGTH = 50001 + HEADER_SIZE;




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


class PartialByteArray {
  public DataType dataType;
  public int initialIndex;
  public byte[] message;
  public int messageStart;
  public int messageEnd;
  
  public PartialByteArray(byte[] rawMessage, int length) {
    dataType = DataType.fromByte(rawMessage[0]);
    
    initialIndex = readBytesAsUInt32(rawMessage, 4, 4);
    
    message = rawMessage;
    messageStart = HEADER_SIZE;
    messageEnd = HEADER_SIZE + readBytesAsUInt32(rawMessage, 8, 4);
  }
  
  private int readBytesAsUInt32(byte[] array, int firstByte, int numBytes) {
    ByteBuffer bigEndianIntReader = ByteBuffer.wrap(array, firstByte, numBytes).order(ByteOrder.LITTLE_ENDIAN);
    long extractedLong = bigEndianIntReader.getInt() & 0xFFFFFFFFL; // HACK to get signed int from bytes, then get rid of sign by keeping the same bits but converting to a long (CAN'T DO `| 0F` BECAUSE IT GETS OPTIMIZED AWAY BY THE COMPILER)
    return (int)extractedLong;
  }
}


class UDPClient {
  private DatagramSocket socket;
  private int serverPort;
  private InetAddress serverAddress;
 
  public UDPClient(String serverAddress, int serverPort) {
    this.serverPort = serverPort;
    try {
      this.socket = new DatagramSocket();
      this.serverAddress = InetAddress.getByName(serverAddress);
    } catch(Exception e) { error(e); }
  }
 
  public void send(Request request) {
    byte[] bytes = request.toString().getBytes();
    DatagramPacket packet = new DatagramPacket(bytes, bytes.length, serverAddress, serverPort);
    try {
      socket.send(packet);
    } catch (IOException e) { error(e); }
  }
  
  public PartialByteArray receive() {
    byte[] message = new byte[MAX_MESSAGE_LENGTH];
    DatagramPacket packet = new DatagramPacket(message, message.length);
    try {
      socket.receive(packet);
    } catch (IOException e) { error(e); }
    return new PartialByteArray(message, packet.getLength());
  }
 
  public void close() {
    socket.close();
  }
}




UDPClient CLIENT;
Request ACTION = Request.CONNECT;


void setup() {
  CLIENT = new UDPClient(SERVER_IP, SERVER_PORT);
  CLIENT.send(ACTION);
  size(640, 480);
}


void display_rgb(PartialByteArray data) {
  for (int b = data.messageStart; b < data.messageEnd; b += 3) {
    color newColor = color(
      data.message[b] & 0xff, // SAME HACK AS BEFORE to convert bytes which are being interpreted as signed into ints
      data.message[b+1] & 0xff,
      data.message[b+2] & 0xff);
      int pixelIndex = (data.initialIndex/3) + ((b - data.messageStart)/3);
      pixels[pixelIndex] = newColor;
  }
}


void display_depth(PartialByteArray data) {
for (int b = data.messageStart; b < data.messageEnd; b += 2) {
  int colorIntensity;
  if(data.message[b+1] == 7){
    colorIntensity = 0;
  } else {
    colorIntensity = ((data.message[b] & 0xff) + (data.message[b+1] << 8) ) >> 2;
  }
  color newColor = color(colorIntensity, colorIntensity, colorIntensity);
  int pixelIndex = (data.initialIndex/2) + ((b - data.messageStart)/2);
  pixels[pixelIndex] = newColor;
  }
}


void draw() {
  loadPixels();
  for (int round = 0; round < 7; round++) {
    PartialByteArray data = CLIENT.receive();
    DataType messageType = DataType.fromByte(data.message[0]);
    
    if (messageType == DISPLAY_DATA_TYPE) {
      if ( messageType == DataType.RGB ) {
        display_rgb(data);
      } else if ( messageType == DataType.DEPTH ) {
        display_depth(data);
      }
    }
  }
  updatePixels();
}
