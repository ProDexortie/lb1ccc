using UnityEngine;
using UnityEngine.XR;
using UnityEngine.XR.Interaction.Toolkit;

public class VRPlayerController : MonoBehaviour
{
    [Header("VR Setup")]
    public Transform cameraOffset;
    public XRRig xrRig;
    
    [Header("Vehicle Following")]
    public VehicleMovement targetVehicle;
    public Vector3 followOffset = new Vector3(0, 2, -5);
    public float followSpeed = 2f;
    public bool followVehicle = true;
    
    [Header("Manual Movement")]
    public float moveSpeed = 3f;
    public InputActionReference moveAction;
    
    private Vector3 targetPosition;
    private bool isVRActive = false;
    
    void Start()
    {
        // Check if VR is available and active
        isVRActive = XRSettings.enabled && XRSettings.loadedDeviceName != "";
        
        if (!isVRActive)
        {
            Debug.Log("VR not detected, using fallback camera controls");
        }
        
        if (targetVehicle == null)
        {
            targetVehicle = FindObjectOfType<VehicleMovement>();
        }
    }
    
    void Update()
    {
        if (followVehicle && targetVehicle != null)
        {
            FollowVehicle();
        }
        else if (!isVRActive)
        {
            HandleNonVRInput();
        }
    }
    
    void FollowVehicle()
    {
        Vector3 desiredPosition = targetVehicle.transform.position + followOffset;
        
        if (isVRActive && xrRig != null)
        {
            // Move XR Rig to follow vehicle
            Vector3 newPosition = Vector3.Lerp(xrRig.transform.position, desiredPosition, followSpeed * Time.deltaTime);
            xrRig.transform.position = newPosition;
        }
        else
        {
            // Move regular camera
            Vector3 newPosition = Vector3.Lerp(transform.position, desiredPosition, followSpeed * Time.deltaTime);
            transform.position = newPosition;
            
            // Look at vehicle
            Vector3 lookDirection = (targetVehicle.transform.position - transform.position).normalized;
            if (lookDirection != Vector3.zero)
            {
                transform.rotation = Quaternion.LookRotation(lookDirection);
            }
        }
    }
    
    void HandleNonVRInput()
    {
        // Basic WASD movement for non-VR testing
        float horizontal = Input.GetAxis("Horizontal");
        float vertical = Input.GetAxis("Vertical");
        
        Vector3 movement = new Vector3(horizontal, 0, vertical) * moveSpeed * Time.deltaTime;
        transform.Translate(movement);
        
        // Mouse look
        if (Input.GetMouseButton(1)) // Right mouse button
        {
            float mouseX = Input.GetAxis("Mouse X");
            float mouseY = Input.GetAxis("Mouse Y");
            
            transform.Rotate(-mouseY, mouseX, 0);
        }
        
        // Toggle vehicle following with F key
        if (Input.GetKeyDown(KeyCode.F))
        {
            followVehicle = !followVehicle;
            Debug.Log("Vehicle following: " + followVehicle);
        }
    }
    
    public void SetFollowVehicle(bool follow)
    {
        followVehicle = follow;
    }
    
    public void SetFollowOffset(Vector3 offset)
    {
        followOffset = offset;
    }
    
    // Method to handle VR controller input for interacting with UI
    public void OnTriggerPressed()
    {
        // This can be connected to XR controller trigger events
        // for interacting with the question UI
        Debug.Log("VR Trigger pressed");
    }
    
    // Method to teleport to a specific position
    public void TeleportTo(Vector3 position)
    {
        if (isVRActive && xrRig != null)
        {
            xrRig.transform.position = position;
        }
        else
        {
            transform.position = position;
        }
    }
}