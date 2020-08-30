using System.Collections;
using System.Collections.Generic;
using UnityEngine;
using System;
using UnityEngine.Profiling;
using Unity.Jobs;
using Unity.Collections;

public struct GenerateMeshValuesJob : IJob
{
    public NativeArray<int> depthValues;
    public int WIDTH;
    public int HEIGHT;
    public float edgeSize;

    public NativeArray<Vector3> vertices;
    public NativeArray<Vector2> uv;
    public NativeArray<int> triangles;
    public NativeArray<int> trianglesUsedLength;

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
    bool validPoint(int index, NativeArray<int> depthValues)
    {
        return depthValues[index] < 2047;/* && (
            (index % WIDTH == WIDTH - 1 || Math.Abs(depthValues[index] - depthValues[index + 1]) < edgeSize) &&
            (index / WIDTH == HEIGHT - 1 || Math.Abs(depthValues[index] - depthValues[index + WIDTH]) < edgeSize));*/
    }

    public void Execute()
    {
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
                uv[v1] = new Vector2(
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
                        meshTrianglesList.Add(v1);
                        meshTrianglesList.Add(v3);
                        meshTrianglesList.Add(v2);

                    }
                    if (validPoint(v4, depthValues))
                    {
                        meshTrianglesList.Add(v2);
                        meshTrianglesList.Add(v3);
                        meshTrianglesList.Add(v4);

                    }
                }
            }
        }
        Profiler.EndSample();

        Profiler.BeginSample("convert triangle List");
        trianglesUsedLength[0] = meshTrianglesList.Count;
        for (int i = 0; i < meshTrianglesList.Count; i++)
        {
            triangles[i] = meshTrianglesList[i];
        }
        Profiler.EndSample();
    }
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

    Vector3[] meshVertices; // Vertices representing points in space for depth values
    Vector2[] meshUvs; // UV mapping of texture onto mesh
    Texture2D texture; // Texture of RGB data
    int mainTextureId;

    JobHandle meshValJobHandle;
    NativeArray<int> trianglesUsedLength;
    NativeArray<int> trianglesNativeArray;
    NativeArray<Vector2> uvNativeArray;
    NativeArray<Vector3> verticesNativeArray;
    NativeArray<int> depthNativeArray;
    bool recievedVisionData;

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
        if ( meshValJobHandle.IsCompleted ) { // Only accept a new frame if the last one is finished processing. Note: Unity makes sure meshValJobHandle can't be null
            Profiler.BeginSample("Begin update vision job");

            meshValJobHandle.Complete(); // Must be called to prevent warning

            // Dispose of old shared mem if it was created
            if (trianglesUsedLength.IsCreated)
            {
                trianglesUsedLength.Dispose();
                depthNativeArray.Dispose();
                trianglesNativeArray.Dispose();
                uvNativeArray.Dispose();
                verticesNativeArray.Dispose();
            }

            GenerateMeshValuesJob meshValJob = new GenerateMeshValuesJob();// Define a generate mesh job
            depthNativeArray = new NativeArray<int>(newDepthValues, Allocator.TempJob); // the 11 bit depth values (input)
            meshValJob.depthValues = depthNativeArray;

            trianglesNativeArray = new NativeArray<int>((WIDTH - 1) * (HEIGHT - 1) * 6, Allocator.TempJob); // Holds triangle data. Note: doesn't have to use it's full length
            meshValJob.triangles = trianglesNativeArray;
            trianglesUsedLength = new NativeArray<int>(1, Allocator.TempJob); // Holds actual length of returned triangle data
            meshValJob.trianglesUsedLength = trianglesUsedLength;
            uvNativeArray = new NativeArray<Vector2>(WIDTH * HEIGHT, Allocator.TempJob); // Holds uv wieghts for texture alignment
            meshValJob.uv = uvNativeArray;
            verticesNativeArray = new NativeArray<Vector3>(WIDTH * HEIGHT, Allocator.TempJob); // Holds the positions of the vertices
            meshValJob.vertices = verticesNativeArray;
            meshValJob.WIDTH = WIDTH;
            meshValJob.HEIGHT = HEIGHT;
            meshValJob.edgeSize = edgeSize;
            meshValJobHandle = meshValJob.Schedule(); // Start running job

            texture = newTexture; //Prepare texture for application once mesh is ready
            recievedVisionData = true;

            Profiler.EndSample();
        }
        
    }

    void Awake()
    {
        mainTextureId = Shader.PropertyToID("_MainTex");
        recievedVisionData = false;
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
        meshVertices = new Vector3[WIDTH * HEIGHT];
        meshUvs = new Vector2[WIDTH * HEIGHT];
        int[] meshTriangles = new int[(WIDTH - 1) * (HEIGHT - 1) * 6];

        // Create grid of vertices and evenly distributed UV texture points
        for (int i = 0; i < WIDTH*HEIGHT; i++)
        {
            int x = i % WIDTH;
            int y = i / WIDTH;
            meshUvs[i] = new Vector2(
                Math.Max((float)(x) / WIDTH, 0f), // - 22
                Math.Min((float)(y) / HEIGHT, 1f) // + 24
            );
            meshVertices[i] = new Vector3((float)x / WIDTH, (float)y / WIDTH, 0f);
        }

        // Apply geometry values to mesh
        mesh = new Mesh();
        mesh.indexFormat = UnityEngine.Rendering.IndexFormat.UInt32;
        mesh.vertices = meshVertices;
        mesh.triangles = meshTriangles;
        mesh.uv = meshUvs;
        meshFilter.mesh = mesh;
        mesh.bounds = new Bounds(new Vector3(0f, 0f, 0f), new Vector3(99f, 99f, 99f));
        meshRenderer.material.shader = shader; // Apply shader to handle transparency
    }

    // Update is called once per frame
    void Update()
    {
        if(recievedVisionData && meshValJobHandle.IsCompleted) // Has recieved data
        {
            Profiler.BeginSample("Update kinect mesh renderer");
            meshValJobHandle.Complete(); // Prevent warning
            mesh.vertices = verticesNativeArray.ToArray();

            int[] trianglesArray = new int[trianglesUsedLength[0]]; // Trim and assign new triangles data
            Array.Copy(trianglesNativeArray.ToArray(), trianglesArray, trianglesUsedLength[0]);
            mesh.triangles = trianglesArray; 

            mesh.uv = uvNativeArray.ToArray();
            meshRenderer.material.SetTexture(mainTextureId, texture);
            meshFilter.mesh = mesh;

            trianglesUsedLength.Dispose();
            depthNativeArray.Dispose();
            trianglesNativeArray.Dispose();
            uvNativeArray.Dispose();
            verticesNativeArray.Dispose();
            recievedVisionData = false;
            Profiler.EndSample();
        }
    }
}
