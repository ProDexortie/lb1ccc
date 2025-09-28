using UnityEngine;
using System.Collections;

namespace VRTrafficEdu
{
    public class VehicleMovement : MonoBehaviour
{
    [Header("Movement Settings")]
    public float speed = 5f;
    public Transform[] waypoints;
    public bool loopRoute = true;
    
    [Header("Vehicle Settings")]
    public Transform vehicleBody;
    public float rotationSpeed = 2f;
    
    private int currentWaypointIndex = 0;
    private bool isMoving = true;
    private TrafficQuestionSystem questionSystem;
    
    void Start()
    {
        questionSystem = FindObjectOfType<TrafficQuestionSystem>();
        
        if (waypoints.Length == 0)
        {
            Debug.LogWarning("No waypoints assigned to vehicle!");
            return;
        }
        
        // Position vehicle at first waypoint
        transform.position = waypoints[0].position;
    }
    
    void Update()
    {
        if (!isMoving || waypoints.Length == 0) return;
        
        MoveTowardsWaypoint();
    }
    
    void MoveTowardsWaypoint()
    {
        Transform targetWaypoint = waypoints[currentWaypointIndex];
        Vector3 direction = (targetWaypoint.position - transform.position).normalized;
        
        // Move vehicle
        transform.position += direction * speed * Time.deltaTime;
        
        // Rotate vehicle to face movement direction
        if (direction != Vector3.zero && vehicleBody != null)
        {
            Quaternion targetRotation = Quaternion.LookRotation(direction);
            vehicleBody.rotation = Quaternion.Slerp(vehicleBody.rotation, targetRotation, rotationSpeed * Time.deltaTime);
        }
        
        // Check if reached waypoint
        if (Vector3.Distance(transform.position, targetWaypoint.position) < 0.5f)
        {
            OnWaypointReached();
        }
    }
    
    void OnWaypointReached()
    {
        currentWaypointIndex++;
        
        // Check if we should trigger a question at this waypoint
        if (questionSystem != null && Random.Range(0f, 1f) < 0.3f) // 30% chance
        {
            StopVehicle();
            questionSystem.ShowRandomQuestion();
        }
        
        // Handle end of route
        if (currentWaypointIndex >= waypoints.Length)
        {
            if (loopRoute)
            {
                currentWaypointIndex = 0;
            }
            else
            {
                isMoving = false;
                Debug.Log("Vehicle reached final destination!");
            }
        }
    }
    
    public void StopVehicle()
    {
        isMoving = false;
    }
    
    public void ResumeVehicle()
    {
        isMoving = true;
    }
    
    public void SetSpeed(float newSpeed)
    {
        speed = Mathf.Max(0f, newSpeed);
    }
    
    // Called when question is answered
    public void OnQuestionAnswered()
    {
        StartCoroutine(ResumeAfterDelay(2f));
    }
    
    private IEnumerator ResumeAfterDelay(float delay)
    {
        yield return new WaitForSeconds(delay);
        ResumeVehicle();
    }
}
}