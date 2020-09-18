using UnityEngine;
using System.Net.Sockets;
using System.Net;
using System.Text;
using System;
using System.Collections;
using System.Collections.Generic;
using System.Threading;
using UnityEngine.Profiling;

public struct TcpMessage
{
    private const int HEADER_SIZE = 5;

    public TcpMessage(byte type_, byte[] content_)
    {
        type = type_;
        content = content_;
    }

    public byte type { get; }
    public byte[] content { get; }

    public byte[] ToBytes()
    {
        List<byte> message = new List<byte>();
        message.Add(type);
        message.AddRange(BitConverter.GetBytes((UInt32)content.Length + HEADER_SIZE));
        message.AddRange(content);
        return message.ToArray();
    }


}

public class TCPManager : MonoBehaviour
{
    [Tooltip("Width of kinect data. Default: 640")]
    public int WIDTH = 640;

    [Tooltip("Height of kinect data. Default: 480")]
    public int HEIGHT = 480;

    [Tooltip("The port of the server. Default: 6969")]
    public int serverOutputPort = 6969;

    [Tooltip("The port of the server. Default: 7878")]
    public int serverInputPort = 7878;

    [Tooltip("The ip of the server. Default: 192.168.1.103")]
    public string serverIp = "192.168.1.103";

    [Tooltip("Angle of the kinect that data is being recieved from. Has to be manually set at the moment. Default: 0f")]
    public float kinectAngle = 0f;

    [Tooltip("The height of the kinect in meters. Default: 1.75f")]
    public float kinectHeight = 1.75f;

    private const int HEADER_SIZE = 5;

    TcpClient sender;
    TcpClient receiver;

    Thread messageThread; // Thread for retrieving data from the server
    static readonly object lockObject = new object(); // Mutual exclusion lock for udpData variable
    bool processData = false; // Signals when new UDP data has been retrieved
    bool runMessageThread; // Boolean to tell thread to stop running on application quit

    TcpMessage newMessage; // New TCP message data

    public GameObject kinectRendererObject; // Prefab to instantiate for rendering kinect data
    public KinectMeshRenderer kinectMeshRenderer;

    /*
     * Updates the mesh or mesh's texture with the packetData array
     */
    void retrieveData(TcpMessage message)
    {
        if (message.type != (byte)'r' && message.type != (byte)'d')
        {
            throw new Exception();
        }

        // handle RGB data
        if (message.type == (byte)'r')
        {
            Texture2D texture = new Texture2D(WIDTH, HEIGHT, TextureFormat.RGBA32, false);
            Color[] texturePixels = new Color[WIDTH * HEIGHT];
            for (int i = 0; i < WIDTH * HEIGHT; i++)
            {
                texturePixels[i] = new Color(
                    (float)message.content[i * 3] / 255,
                    (float)message.content[i * 3 + 1] / 255,
                    (float)message.content[i * 3 + 2] / 255,
                    1f
                );
            }
            texture.SetPixels(texturePixels);
            texture.Apply();
            kinectMeshRenderer.updateVision(texture);
        }

        // Handle depth data
        else if (message.type == (byte)'d')
        {
            int[] depthValues = new int[WIDTH * HEIGHT];
            for (int i = 0; i < WIDTH*HEIGHT; i++)
            {
                int depth = (int)(message.content[i*2] + (message.content[i*2 + 1] << 8));
                depthValues[i] = depth;
            }
            kinectMeshRenderer.updateVision(depthValues);
        }
    }

    /* No longer being used atm */
    private Vector3 polarToCartesian(float x, float y, float dist)
    {
        Vector3 origin = new Vector3(0, 0, dist);
        Quaternion rotation = Quaternion.Euler(x, y, 0);
        return rotation * origin;
    }

    /*
     * Start UDP client then continuously
     * gather 32 messages for processing at a time (32 messages would equal a full frame of new rgb and depth data if you ignore that a lot get skipped)
     */
    private void MessageThreadMethod()
    {
        receiver = new TcpClient(serverIp, serverOutputPort);//endPoint); //endPoint
        sender = new TcpClient(serverIp, serverInputPort);

        using (NetworkStream stream = receiver.GetStream())
        {
            while (runMessageThread)
            {
                byte[] header = new byte[HEADER_SIZE];
                stream.Read(header, 0, HEADER_SIZE);
                uint dataLength = BitConverter.ToUInt32(header, 1) - HEADER_SIZE;

                byte[] contentData = new byte[dataLength];
                int readBytes = 0;
                while (readBytes < dataLength)
                {
                    readBytes += stream.Read(contentData, readBytes, (int)dataLength - readBytes);
                }
                lock (lockObject)
                {
                     newMessage = new TcpMessage((byte)header[0], contentData);
                     processData = true;
                }
            }
            receiver.Close();
        }
    }

    /*
     * Create a new kinect mesh rendering object that new data will be sent to
     */
    public void createNewKinectMesh()
    {
        kinectMeshRenderer = Instantiate(
                kinectRendererObject,
                new Vector3(0, kinectHeight, 0),
                Quaternion.Euler(kinectAngle, 0f, 0f)
            ).GetComponent<KinectMeshRenderer>();
    }

    /*
     * Send a message to the kinect server requesting a new tilt angle
     */
    public void requestTiltChange(int newTilt)
    {
        byte[] messageData = new TcpMessage((byte)'t', new byte[] { (byte)newTilt }).ToBytes();
            
            /*new byte[] {
            (byte) 't',
            (byte) 6,
            (byte) 0,
            (byte) 0,
            (byte) 0,
            (byte) newTilt };*/

        kinectAngle = 0f-newTilt;

        createNewKinectMesh();
        sender.GetStream().Write(messageData, 0, messageData.Length);
        Debug.Log("SENT TILT REQUEST: " + newTilt);
    }

    void Awake()
    {
       
    }

    void Start()
    {
        createNewKinectMesh();

        // Start TCP reciever thread
        runMessageThread = true;
        messageThread = new Thread(new ThreadStart(MessageThreadMethod));
        messageThread.Start();
    }

    void Update()
    {
        if (processData)
        {
            lock (lockObject)
            {
                Profiler.BeginSample("Retrieve message data");

                retrieveData(newMessage);
                processData = false;
                
                Profiler.EndSample();
            }
        }
    }

    void OnApplicationQuit()
    {
        runMessageThread = false;
        
        if (sender != null)
        {
            byte[] disconnectMessage = new byte[] { (byte)'d', (byte)0 };
            sender.GetStream().Write(disconnectMessage, 0, disconnectMessage.Length);
            sender.Close();
        }        
    }
}
