using System.Collections;
using System.Collections.Generic;
using UnityEngine;
using System;

[RequireComponent(typeof(MeshRenderer), typeof(MeshFilter))]
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

    /*
     * Converts a raw kinect depth reading to the depth in meters
     * Invalid depths return 8m since this is further than the kinect can actually read
     * (although such vertices should probably never wind up being used by the mesh)
     */
    float rawDepthToMeters(int depthValue)
    {
        if (depthValue < 2047)
        {
            return (float)(1.0 / ((double)(depthValue) * -0.0030711016 + 3.3309495161));
        }
        return 8f;
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
        return depthValues[index] < 2047 && (
            (index % WIDTH == WIDTH - 1 || Math.Abs(depthValues[index] - depthValues[index + 1]) < edgeSize) &&
            (index / WIDTH == HEIGHT - 1 || Math.Abs(depthValues[index] - depthValues[index + WIDTH]) < edgeSize));
    }

    /* reposition mesh vertices based on new depth values,
     * create triangles for valid vertices,
     * update RGB texture,
     * update texture UV mapping (eventually this should be dependent on depth for minimal horizontal disparity)
     */
    public void updateVision(Texture2D newTexture, int[] newDepthValues)
    {
        texture = newTexture;

        /*
        // Old code for adding a grid on top of texture data
        for(int j = 0; j < WIDTH; j++)
        {
            for(int k = 0; k < HEIGHT; k++)
            {
                if( k % 10 == 0 || j % 10 == 0)
                {
                    if( k % 100 == 0 || j % 100 == 0)
                    {
                        texture.SetPixel(j, k, new Color(0f,0f, 1f ,1f));
                    }
                    else
                    {
                        texture.SetPixel(j, k, new Color(1f, 1f, 1f, 1f));
                    }
                }
            }
        }
        */

        List<int> meshTrianglesList = new List<int>();
        for (int i = 0; i < WIDTH * HEIGHT; i++)
        {
            int x = i % WIDTH;
            int y = i / WIDTH;
            meshVertices[i] = depthToWorld(x, y, newDepthValues[i]);
        }

        for (int x = 0; x < WIDTH - 1; x++)
        {
            for (int y = 0; y < HEIGHT - 1; y++)
            {
                int v1 = x + y * WIDTH;
                meshUvs[x + y * WIDTH] = new Vector2(
                    Math.Max(Math.Min((x + 0.07777777777777778f * x - 16.6666666f) / WIDTH, 1f), 0f),
                    Math.Min((y - 0.08571428571428572f * y + 46.7142857f) / HEIGHT, 1f)
                );

                int v2 = x + 1 + y * WIDTH;
                int v3 = x + (y + 1) * WIDTH;
                int v4 = x + 1 + (y + 1) * WIDTH;
                if (validPoint(v2, newDepthValues) && validPoint(v3, newDepthValues))
                {
                    if (validPoint(v1, newDepthValues))
                    {
                        meshTrianglesList.Add(v1);
                        meshTrianglesList.Add(v3);
                        meshTrianglesList.Add(v2);

                    }
                    if (validPoint(v4, newDepthValues))
                    {
                        meshTrianglesList.Add(v2);
                        meshTrianglesList.Add(v3);
                        meshTrianglesList.Add(v4);

                    }
                }
            }
        }
        mesh.vertices = meshVertices;
        mesh.triangles = meshTrianglesList.ToArray();
        mesh.RecalculateBounds();
        mesh.uv = meshUvs;

        texture.Apply();
        meshRenderer.material.SetTexture("_MainTex", texture);
        meshFilter.mesh = mesh;
    }

    void Awake()
    {
        meshRenderer = GetComponent<MeshRenderer>();
        meshFilter = GetComponent<MeshFilter>();
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
        for (int y = 0; y < HEIGHT; y++)
        {
            for (int x = 0; x < WIDTH; x++)
            {
                int vIndex = (y * WIDTH) + x;
                meshUvs[vIndex] = new Vector2(
                     Math.Max((float)(x) / WIDTH, 0f), // - 22
                     Math.Min((float)(y) / HEIGHT, 1f) // + 24
                    );
            }
        }

        // Apply geometry values to mesh
        mesh = new Mesh();
        mesh.indexFormat = UnityEngine.Rendering.IndexFormat.UInt32;
        mesh.vertices = meshVertices;
        mesh.triangles = meshTriangles;
        mesh.uv = meshUvs;
        meshFilter.mesh = mesh;
        meshRenderer.material.shader = shader; // Apply shader to handle transparency
    }

    // Update is called once per frame
    void Update()
    {

    }
}
