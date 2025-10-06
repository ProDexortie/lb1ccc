using UnityEngine;

namespace VRTrafficEdu
{
    public class CityGenerator : MonoBehaviour
{
    [Header("City Layout")]
    public int citySize = 10;
    public float blockSize = 20f;
    public float roadWidth = 4f;
    
    [Header("Building Settings")]
    public GameObject buildingPrefab;
    public float minBuildingHeight = 3f;
    public float maxBuildingHeight = 15f;
    public int buildingsPerBlock = 4;
    
    [Header("Road Settings")]
    public Material roadMaterial;
    public Material sidewalkMaterial;
    
    [Header("Traffic Signs")]
    public GameObject[] trafficSignPrefabs;
    public int trafficSignCount = 6;
    
    void Start()
    {
        GenerateCity();
    }
    
    void GenerateCity()
    {
        CreateRoadNetwork();
        CreateBuildings();
        CreateTrafficSigns();
        CreateWaypoints();
    }
    
    void CreateRoadNetwork()
    {
        // Create main roads
        for (int x = 0; x <= citySize; x++)
        {
            CreateRoad(
                new Vector3(x * blockSize - citySize * blockSize / 2, 0, -citySize * blockSize / 2),
                new Vector3(x * blockSize - citySize * blockSize / 2, 0, citySize * blockSize / 2),
                roadWidth
            );
        }
        
        for (int z = 0; z <= citySize; z++)
        {
            CreateRoad(
                new Vector3(-citySize * blockSize / 2, 0, z * blockSize - citySize * blockSize / 2),
                new Vector3(citySize * blockSize / 2, 0, z * blockSize - citySize * blockSize / 2),
                roadWidth
            );
        }
    }
    
    void CreateRoad(Vector3 start, Vector3 end, float width)
    {
        Vector3 direction = (end - start).normalized;
        float length = Vector3.Distance(start, end);
        Vector3 center = (start + end) / 2;
        
        // Create road
        GameObject road = GameObject.CreatePrimitive(PrimitiveType.Cube);
        road.name = "Road";
        road.tag = "Road";
        road.layer = LayerMask.NameToLayer("Ground");
        
        road.transform.position = center;
        road.transform.localScale = new Vector3(width, 0.1f, length);
        road.transform.LookAt(end);
        
        if (roadMaterial != null)
        {
            road.GetComponent<Renderer>().material = roadMaterial;
        }
        else
        {
            road.GetComponent<Renderer>().material.color = Color.gray;
        }
    }
    
    void CreateBuildings()
    {
        for (int x = 0; x < citySize; x++)
        {
            for (int z = 0; z < citySize; z++)
            {
                Vector3 blockCenter = new Vector3(
                    x * blockSize - citySize * blockSize / 2 + blockSize / 2,
                    0,
                    z * blockSize - citySize * blockSize / 2 + blockSize / 2
                );
                
                CreateBuildingsInBlock(blockCenter);
            }
        }
    }
    
    void CreateBuildingsInBlock(Vector3 blockCenter)
    {
        for (int i = 0; i < buildingsPerBlock; i++)
        {
            Vector3 buildingPosition = blockCenter + new Vector3(
                Random.Range(-blockSize / 3, blockSize / 3),
                0,
                Random.Range(-blockSize / 3, blockSize / 3)
            );
            
            CreateBuilding(buildingPosition);
        }
    }
    
    void CreateBuilding(Vector3 position)
    {
        GameObject building;
        
        if (buildingPrefab != null)
        {
            building = Instantiate(buildingPrefab, position, Quaternion.identity);
        }
        else
        {
            building = GameObject.CreatePrimitive(PrimitiveType.Cube);
            building.name = "Building";
        }
        
        building.tag = "Building";
        building.layer = LayerMask.NameToLayer("Buildings");
        
        float height = Random.Range(minBuildingHeight, maxBuildingHeight);
        building.transform.localScale = new Vector3(
            Random.Range(3f, 8f),
            height,
            Random.Range(3f, 8f)
        );
        
        building.transform.position = new Vector3(position.x, height / 2, position.z);
        
        // Random building color
        Color buildingColor = new Color(
            Random.Range(0.6f, 0.9f),
            Random.Range(0.6f, 0.9f),
            Random.Range(0.6f, 0.9f)
        );
        building.GetComponent<Renderer>().material.color = buildingColor;
    }
    
    void CreateTrafficSigns()
    {
        for (int i = 0; i < trafficSignCount; i++)
        {
            Vector3 signPosition = new Vector3(
                Random.Range(-citySize * blockSize / 2, citySize * blockSize / 2),
                0,
                Random.Range(-citySize * blockSize / 2, citySize * blockSize / 2)
            );
            
            CreateTrafficSign(signPosition);
        }
    }
    
    void CreateTrafficSign(Vector3 position)
    {
        GameObject sign;
        
        if (trafficSignPrefabs.Length > 0)
        {
            int randomIndex = Random.Range(0, trafficSignPrefabs.Length);
            sign = Instantiate(trafficSignPrefabs[randomIndex], position, Quaternion.identity);
        }
        else
        {
            // Create a simple sign using primitives
            sign = new GameObject("Traffic Sign");
            
            // Sign post
            GameObject post = GameObject.CreatePrimitive(PrimitiveType.Cylinder);
            post.transform.parent = sign.transform;
            post.transform.localPosition = Vector3.zero;
            post.transform.localScale = new Vector3(0.1f, 1.5f, 0.1f);
            post.GetComponent<Renderer>().material.color = Color.gray;
            
            // Sign board
            GameObject board = GameObject.CreatePrimitive(PrimitiveType.Cube);
            board.transform.parent = sign.transform;
            board.transform.localPosition = new Vector3(0, 2.5f, 0);
            board.transform.localScale = new Vector3(1f, 0.8f, 0.1f);
            board.GetComponent<Renderer>().material.color = Color.red;
        }
        
        sign.tag = "TrafficSign";
        sign.layer = LayerMask.NameToLayer("TrafficSigns");
        sign.transform.position = new Vector3(position.x, 0, position.z);
    }
    
    void CreateWaypoints()
    {
        GameObject waypointsParent = new GameObject("Waypoints");
        
        // Create a simple route through the city
        Vector3[] waypointPositions = new Vector3[]
        {
            new Vector3(-citySize * blockSize / 4, 0.5f, -citySize * blockSize / 4),
            new Vector3(citySize * blockSize / 4, 0.5f, -citySize * blockSize / 4),
            new Vector3(citySize * blockSize / 4, 0.5f, citySize * blockSize / 4),
            new Vector3(-citySize * blockSize / 4, 0.5f, citySize * blockSize / 4),
        };
        
        Transform[] waypoints = new Transform[waypointPositions.Length];
        
        for (int i = 0; i < waypointPositions.Length; i++)
        {
            GameObject waypoint = new GameObject($"Waypoint_{i}");
            waypoint.transform.parent = waypointsParent.transform;
            waypoint.transform.position = waypointPositions[i];
            waypoints[i] = waypoint.transform;
            
            // Add a small visual indicator (can be disabled in builds)
            GameObject indicator = GameObject.CreatePrimitive(PrimitiveType.Sphere);
            indicator.transform.parent = waypoint.transform;
            indicator.transform.localPosition = Vector3.zero;
            indicator.transform.localScale = Vector3.one * 0.5f;
            indicator.GetComponent<Renderer>().material.color = Color.yellow;
        }
        
        // Assign waypoints to vehicle if it exists
        VehicleMovement vehicle = FindObjectOfType<VehicleMovement>();
        if (vehicle != null)
        {
            vehicle.waypoints = waypoints;
        }
    }
}
}