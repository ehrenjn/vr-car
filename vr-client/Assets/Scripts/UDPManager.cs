using UnityEngine;
using System.Net.Sockets;
using System.Net;
using System.Text;
using System;
using System.Collections;
using System.Collections.Generic;
using System.Threading;
using UnityEngine.Profiling;

public class UDPManager : MonoBehaviour
{
    [Tooltip("Width of kinect data. Default: 640")]
    public int WIDTH = 640;

    [Tooltip("Height of kinect data. Default: 480")]
    public int HEIGHT = 480;

    [Tooltip("The port of the server. Default: 6969")]
    public int serverPort = 6969;

    [Tooltip("The ip of the server. Default: 192.168.1.103")]
    public string serverIp = "192.168.1.103";

    [Tooltip("Angle of the kinect that data is being recieved from. Has to be manually set at the moment. Default: 0f")]
    public float kinectAngle = 0f;

    [Tooltip("The height of the kinect in meters. Default: 1.75f")]
    public float kinectHeight = 1.75f;


    Texture2D texture; // Texture containing newest processed texture data
    int[] depthValues; // Newest depth values

    Thread thread; // Thread for UDP server
    static readonly object lockObject = new object(); // Mutual exclusion lock for udpData variable
    bool processData = false; // Signals when new UDP data has been retrieved
    bool runThread; // Boolean to tell thread to stop running on application quit

    List<byte[]> udpData; // New UDP messages
    const int HEADER_SIZE = 12;

    public GameObject kinectRendererObject; // Prefab to instantiate for rendering kinect data
    public KinectMeshRenderer kinectMeshRenderer;

    /*
     * Updates the mesh or mesh's texture with the packetData array
     */
    void retrieveData(byte[] packetData)
    {
        uint index = BitConverter.ToUInt32(packetData, 4);
        uint length = BitConverter.ToUInt32(packetData, 8);

        if ((packetData[0] != (byte)'r' && packetData[0] != (byte)'d') || length > 65535)
        {
            throw new Exception();
        }

        // handle RGB data
        if (packetData[0] == (byte)'r')
        {

            for (int i = 0; i < length; i += 3)
            {
                int x = (int)(((i + index) / 3) % WIDTH);
                int y = (int)(((i + index) / 3) / WIDTH);
                texture.SetPixel(
                x,
                y,
                new Color(
                    (float)packetData[i + HEADER_SIZE] / 255,
                    (float)packetData[i + HEADER_SIZE + 1] / 255,
                    (float)packetData[i + HEADER_SIZE + 2] / 255,
                    1f
                ));
            }
        }

        // Handle depth data
        else if (packetData[0] == (byte)'d')
        {
            for (int i = 0; i < length / 2; i++)
            {
                int depth = (int)(packetData[i * 2 + HEADER_SIZE] + (packetData[i * 2 + HEADER_SIZE + 1] << 8));
                depthValues[i + index / 2] = depth;
            }
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
    UdpClient udpClient;
    private void ThreadMethod()
    {
        udpClient = new UdpClient(serverPort);
        udpClient.Connect(serverIp, serverPort);

        Byte[] sendBytes = Encoding.ASCII.GetBytes("c");
        udpClient.Send(sendBytes, 1);

        // Blocks until a message returns on this socket from a remote host
        IPEndPoint RemoteIpEndPoint = new IPEndPoint(IPAddress.Any, 0);
        Byte[] receiveBytes = udpClient.Receive(ref RemoteIpEndPoint);

        while (runThread)
        {
            List<byte[]> dataBuffer = new List<byte[]>();
            for (int i = 0; i < 8; i++)
            {
                dataBuffer.Add(udpClient.Receive(ref RemoteIpEndPoint));
            }
            lock (lockObject)
            {
                udpData = dataBuffer;
                processData = true;
            }
        }
        //processData = false; // Todo: see if uncommenting this line fixes occasional errors on exit
    }

    /*
     * Create a new kinect mesh rendering object that new data will be sent to
     */
    public void createNewKinectMesh()
    {
        kinectMeshRenderer = Instantiate(kinectRendererObject, new Vector3(0, kinectHeight, 0), Quaternion.Euler(kinectAngle, 0f, 0f)).GetComponent<KinectMeshRenderer>();
        texture = new Texture2D(WIDTH, HEIGHT, TextureFormat.RGBA32, false);
        Color invisibleColor = new Color(0f, 0f, 0f, 0f);
        Color[] texturePixels = new Color[WIDTH * HEIGHT];
        for (int i = 0; i < WIDTH * HEIGHT; i++)
        {
            texturePixels[i] = invisibleColor;
        }
        texture.SetPixels(texturePixels);
        texture.Apply();
    }

    /*
     * Send a message to the kinect server requesting a new tilt angle
     */
    public void requestTiltChange(int newTilt)
    {
        byte[] messageData = new byte[2] { Encoding.ASCII.GetBytes("t")[0], (byte) newTilt };
        kinectAngle = 0f-newTilt;
        createNewKinectMesh();
        udpClient.Send(messageData, 2);
    }

    void Awake()
    {
       
    }

    void Start()
    {
        // Initialize mesh vertices
        depthValues = new int[WIDTH * HEIGHT];

        // Initialize texture
        createNewKinectMesh();

        // Start UDP server thread
        udpData = new List<byte[]>();
        runThread = true;
        thread = new Thread(new ThreadStart(ThreadMethod));
        thread.Start();
    }

    void Update()
    {
        if (processData)
        {
            lock (lockObject)
            {
                Profiler.BeginSample("Retrieve message data");

                processData = false;
                foreach (byte[] message in udpData)
                {
                    retrieveData(message);
                }
                texture.Apply();
                kinectMeshRenderer.updateVision(texture, depthValues);

                Profiler.EndSample();
            }
        }
    }

    void OnApplicationQuit()
    {
        runThread = false;
        // udpClient.Send(Encoding.ASCII.GetBytes("d"), 1); // Note: uncommenting this line will cause you to need to restart the kinect server atm
    }
}
