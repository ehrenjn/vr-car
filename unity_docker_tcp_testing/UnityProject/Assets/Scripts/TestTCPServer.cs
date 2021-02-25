using UnityEngine;
using System;
using System.IO;
using System.Net;
using System.Net.Sockets;
using System.Text;
using System.Threading;

public class TestTCPServer : MonoBehaviour
{

    Thread messageThread;

    static void Connect(string message)
    {
        TcpListener listener = null;
        TcpClient client;
        NetworkStream stream;
        try
        {
            Byte[] data = System.Text.Encoding.ASCII.GetBytes(message);
            Byte[] header = { 0, (byte)((data.Length + 5) % 256), (byte)((data.Length + 5) >> 8), 0, 0 };
            Byte[] full = new Byte[header.Length + data.Length];
            header.CopyTo(full, 0);
            data.CopyTo(full, header.Length);

            Int32 port = 7787;

            listener = new TcpListener(IPAddress.Any, port);
            //listener = new TcpListener(IPAddress.Parse(server), port);
            listener.Start();

            Debug.Log("waiting for client...");
            client = listener.AcceptTcpClient();
            Debug.Log("get stream...");
            stream = client.GetStream();
            Debug.Log("read...");



            Byte[] responseBytes = new Byte[256];
            Int32 bytes = stream.Read(responseBytes, 0, 5);
            int readLen = (int)responseBytes[1] + (int)responseBytes[2] * 256 - 5;
            bytes = stream.Read(responseBytes, 0, readLen);

            String responseData = System.Text.Encoding.ASCII.GetString(responseBytes, 0, bytes);
            Debug.Log("Received: " + responseData);

            stream.Write(full, 0, full.Length);
            Debug.Log("Sent: " + message);

            // Close everything.
            stream.Close();
            client.Close();
        }
        catch (ArgumentNullException e)
        {
            Debug.Log("ArgumentNullException:");
            Debug.Log(e);
        }
        catch (SocketException e)
        {
            Debug.Log("SocketException:");
            Debug.Log(e);
        }
        finally
        {
            if (!(listener is null) && listener.Server.IsBound)
            {
                listener.Stop();

            }
            
            
        }
    }

    public static void Main()
    {
        Connect( "response");
    }


    // Start is called before the first frame update
    void Start()
    {
        messageThread = new Thread(new ThreadStart(Main));
        messageThread.Start();
    }

    // Update is called once per frame
    void Update()
    {

    }
}
