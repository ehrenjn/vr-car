using System.Collections;
using System.Collections.Generic;
using UnityEngine;
using System;

public class UserManager : MonoBehaviour
{

    public int currentAngle = 0;
    TCPManager tcpManager;

    // Start is called before the first frame update
    void Start()
    {
        tcpManager = GetComponent<TCPManager>();
    }

    void attemptTiltChange(int newTilt)
    {
        newTilt = Math.Max(-30,Math.Min(30, newTilt));
        if (newTilt != currentAngle)
        {
            currentAngle = newTilt;
            tcpManager.requestTiltChange(currentAngle);
        }
    }

    // Update is called once per frame
    void Update()
    {
        if (Input.GetKeyDown("up"))
        {
            Debug.Log("Up key pressed");
            attemptTiltChange(currentAngle + 30);
        }
        else if (Input.GetKeyDown("down"))
        {
            attemptTiltChange(currentAngle - 30);
        }
        else if (Input.GetKeyDown("c"))
        {
            Debug.Log("Clear key pressed");
        }
    }
}
