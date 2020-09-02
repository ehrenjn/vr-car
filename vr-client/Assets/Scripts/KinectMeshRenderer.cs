using System.Collections;
using System.Collections.Generic;
using UnityEngine;
using System;
using UnityEngine.Profiling;
using System.Threading;

public enum MeshState //The state of async mesh generation
{
    HasNewData, // New mesh data that should be applied next frame
    HasOldData, // No new data (has been applied to renderer already)
    GeneratingData // Currently generating new mesh data
}

public class KinectMeshRenderer : MonoBehaviour
{
    [Tooltip("Width of kinect data. Default: 640")]
    public int WIDTH = 640;

    [Tooltip("Height of kinect data. Default: 480")]
    public int HEIGHT = 480;

    [Tooltip("The minimum raw difference to a neighbouring depth value that is considered a split edge")]
    public float edgeSize = 24;

    [Tooltip("Shader to use for procedural mesh. If not specified the Unlit/Transparent shader will be used")]
    public Shader shader;

    MeshFilter meshFilter; // This object's MeshFilter component
    MeshRenderer meshRenderer; // This object's MeshRenderer component
    Mesh mesh; // The mesh to be rendered

    Texture2D texture; // Texture of RGB data
    int mainTextureId; // The ID representing a main texture (used to set new textures to the kinect mesh's material)

    MeshState meshState; //The state of async mesh generation
    Vector3[] vertices;
    Vector2[] uvs;
    int[] triangles;

    public void asyncGenerateMesh(int[] depthValues)
    {
        vertices = new Vector3[WIDTH * HEIGHT];
        uvs = new Vector2[WIDTH * HEIGHT];
        List<int> trianglesList = new List<int>();

        Profiler.BeginSample("Get vertices");
        for (int i = 0; i < WIDTH * HEIGHT; i++)
        {
            int x = i % WIDTH;
            int y = i / WIDTH;
            vertices[i] = depthToWorld(x, y, depthValues[i]);
        }
        Profiler.EndSample();

        Profiler.BeginSample("Get uv and triangles");
        List<int> meshTrianglesList = new List<int>();
        for (int x = 0; x < WIDTH - 1; x++)
        {
            for (int y = 0; y < HEIGHT - 1; y++)
            {
                int v1 = x + y * WIDTH;
                uvs[v1] = new Vector2(
                    Math.Max(Math.Min((1.07777777777777778f * x - 16.6666666f) / WIDTH, 1f), 0f),
                    Math.Min((0.9142857142857143f * y + 46.7142857f) / HEIGHT, 1f)  //y - 0.08571428571428572f * y + 46.7142857f
                );

                int v2 = v1 + 1;
                int v3 = v1 + WIDTH;

                if (validPoint(v2, depthValues) && validPoint(v3, depthValues))
                {
                    int v4 = v3 + 1;
                    if (validPoint(v1, depthValues))
                    {
                        trianglesList.Add(v1);
                        trianglesList.Add(v3);
                        trianglesList.Add(v2);
                    }
                    if (validPoint(v4, depthValues))
                    {
                        trianglesList.Add(v2);
                        trianglesList.Add(v3);
                        trianglesList.Add(v4);
                    }
                }
            }
        }
        Profiler.EndSample();

        Profiler.BeginSample("convert triangle List");
        triangles = trianglesList.ToArray();
        Profiler.EndSample();

        meshState = MeshState.HasNewData;
    }

    /*
    * Converts a raw kinect depth reading to the depth in meters
    * Invalid depths return 8m since this is further than the kinect can actually read
    * (although such vertices should probably never wind up being used by the mesh)
    */
    float rawDepthToMeters(int depthValue)
    {
        return (float)(1.0 / ((double)(depthValue) * -0.0030711016 + 3.3309495161));
    }

    /*
     * Converts a depth value to a point in space
     */
    Vector3 depthToWorld(int x, int y, int depthValue)
    {

        double fx_d = 1.0 / 5.9421434211923247e+02;
        double fy_d = 1.0 / 5.9104053696870778e+02;
        double cx_d = 3.3930780975300314e+02;
        double cy_d = 2.4273913761751615e+02;

        // Drawing the result vector to give each point its three-dimensional space
        float depth = rawDepthToMeters(depthValue);

        return new Vector3(
             (float)((x - cx_d) * depth * fx_d),
             (float)((y - cy_d) * depth * fy_d),
             (float)(depth)
            );
    }

    /*
     * Determines if a point has a valid depth reading
     * (Valid depth reading and not part of an edge)
     */
    bool validPoint(int index, int[] depthValues)
    {
        return depthValues[index] < 2047;/* && (
            (index % WIDTH == WIDTH - 1 || Math.Abs(depthValues[index] - depthValues[index + 1]) < edgeSize) &&
            (index / WIDTH == HEIGHT - 1 || Math.Abs(depthValues[index] - depthValues[index + WIDTH]) < edgeSize));*/
    }

    /*
     * Updates the rotation of the kinect mesh
     */
    public void updateRotation(float degreesVertical)
    {
        Quaternion.Euler(0, -degreesVertical, 0);
    }

    /* Begin processing vision data in a worker thread
     */
    public void updateVision(Texture2D newTexture, int[] newDepthValues)
    {
        if (meshState == MeshState.HasOldData) { // Only accept a new frame if the last one is finished processing. Note: Unity makes sure meshValJobHandle can't be null
            meshState = MeshState.GeneratingData;
            Profiler.BeginSample("Begin update vision job");
            Thread thread = new Thread(() => asyncGenerateMesh(newDepthValues));
            thread.Start();
            texture = newTexture; //Prepare texture for application once mesh is ready
            Profiler.EndSample();
        }
        
    }

    void Awake()
    {
        meshState = MeshState.HasOldData;
        mainTextureId = Shader.PropertyToID("_MainTex");
        meshRenderer = transform.GetChild(0).GetComponent<MeshRenderer>();
        meshFilter = transform.GetChild(0).GetComponent<MeshFilter>();
        if (!shader)
        {
            shader = Shader.Find("Unlit/Transparent Cutout");
        }
    }

    // Start is called before the first frame update
    void Start()
    {
        // Create mesh's needed arrays
        vertices = new Vector3[WIDTH * HEIGHT];
        uvs = new Vector2[WIDTH * HEIGHT];
        triangles = new int[(WIDTH - 1) * (HEIGHT - 1) * 6];

        // Create grid of vertices and evenly distributed UV texture points
        for (int i = 0; i < WIDTH*HEIGHT; i++)
        {
            int x = i % WIDTH;
            int y = i / WIDTH;
            uvs[i] = new Vector2(
                Math.Max((float)(x) / WIDTH, 0f), // - 22
                Math.Min((float)(y) / HEIGHT, 1f) // + 24
            );
            vertices[i] = new Vector3((float)x / WIDTH, (float)y / WIDTH, 10f);
        }

        // Apply geometry values to mesh
        mesh = new Mesh();
        mesh.indexFormat = UnityEngine.Rendering.IndexFormat.UInt32;
        mesh.vertices = vertices;
        mesh.triangles = triangles;
        mesh.uv = uvs;
        meshFilter.mesh = mesh;
        mesh.bounds = new Bounds(new Vector3(0f, 0f, 0f), new Vector3(99f, 99f, 99f));
        meshRenderer.material.shader = shader; // Apply shader to handle transparency
    }

    // Update is called once per frame
    void Update()
    {
        if(meshState == MeshState.HasNewData) // Has recieved data
        {
            Profiler.BeginSample("Update kinect mesh renderer");
            mesh.vertices = vertices;
            mesh.triangles = triangles;

            mesh.uv = uvs;
            meshRenderer.material.SetTexture(mainTextureId, texture);
            meshFilter.mesh = mesh;

            meshState = MeshState.HasOldData;
            Profiler.EndSample();
        }
    }
}
