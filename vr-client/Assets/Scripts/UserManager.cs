using System.Collections;
using System.Collections.Generic;
using UnityEngine;
using System;

public class PcUserManager : MonoBehaviour
{

    public int currentAngle = 0;
    UDPManager udpManager;

    // Start is called before the first frame update
    void Start()
    {
        udpManager = GetComponent<UDPManager>();
    }

    void attemptTiltChange(int newTilt)
    {
        newTilt = Math.Max(-30,Math.Min(30, newTilt));
        if (newTilt != currentAngle)
        {
            currentAngle = newTilt;
            udpManager.requestTiltChange(currentAngle);
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
