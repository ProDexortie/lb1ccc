using UnityEngine;
using UnityEngine.XR;
using UnityEngine.XR.Management;

namespace VRTrafficEdu
{
    /// <summary>
    /// Manages VR initialization and setup for the traffic education demo
    /// </summary>
    public class VRSetupManager : MonoBehaviour
    {
        [Header("VR Settings")]
        public bool enableVR = true;
        public bool fallbackToNonVR = true;
        
        [Header("Performance Settings")]
        public int targetFrameRate = 90;
        public bool enableFoveatedRendering = true;
        
        private bool isVRInitialized = false;
        
        void Start()
        {
            InitializeVR();
        }
        
        void InitializeVR()
        {
            if (!enableVR)
            {
                Debug.Log("VR disabled by user setting");
                return;
            }
            
            // Try to initialize VR
            var xrSettings = XRGeneralSettings.Instance;
            if (xrSettings == null)
            {
                Debug.LogWarning("XR General Settings not found");
                HandleVRInitializationFailure();
                return;
            }
            
            var xrManager = xrSettings.Manager;
            if (xrManager == null)
            {
                Debug.LogWarning("XR Manager not found");
                HandleVRInitializationFailure();
                return;
            }
            
            if (!xrManager.isInitializationComplete)
            {
                Debug.Log("Initializing XR...");
                StartCoroutine(InitializeXR(xrManager));
            }
            else
            {
                OnVRInitialized();
            }
        }
        
        System.Collections.IEnumerator InitializeXR(XRManagerSettings xrManager)
        {
            yield return xrManager.InitializeLoader();
            
            if (xrManager.activeLoader != null)
            {
                OnVRInitialized();
                yield break;
            }
            
            Debug.LogWarning("Failed to initialize VR");
            HandleVRInitializationFailure();
        }
        
        void OnVRInitialized()
        {
            isVRInitialized = true;
            Debug.Log("VR initialized successfully");
            
            // Apply VR-specific settings
            if (targetFrameRate > 0)
            {
                Application.targetFrameRate = targetFrameRate;
            }
            
            // Enable foveated rendering if supported
            if (enableFoveatedRendering)
            {
                // This would need specific implementation for each VR platform
                Debug.Log("Foveated rendering setting applied");
            }
            
            // Notify other systems that VR is ready
            VRPlayerController playerController = FindObjectOfType<VRPlayerController>();
            if (playerController != null)
            {
                Debug.Log("VR Player Controller found and ready");
            }
        }
        
        void HandleVRInitializationFailure()
        {
            if (fallbackToNonVR)
            {
                Debug.Log("Falling back to non-VR mode");
                isVRInitialized = false;
                
                // Ensure main camera is active for non-VR mode
                Camera mainCamera = Camera.main;
                if (mainCamera != null)
                {
                    mainCamera.gameObject.SetActive(true);
                }
            }
            else
            {
                Debug.LogError("VR initialization failed and fallback disabled");
            }
        }
        
        void OnApplicationPause(bool pauseStatus)
        {
            if (isVRInitialized)
            {
                // Handle VR pause/resume
                Debug.Log($"VR Application pause: {pauseStatus}");
            }
        }
        
        void OnApplicationFocus(bool hasFocus)
        {
            if (isVRInitialized)
            {
                // Handle VR focus changes
                Debug.Log($"VR Application focus: {hasFocus}");
            }
        }
        
        public bool IsVRActive()
        {
            return isVRInitialized && XRSettings.enabled;
        }
        
        public void ToggleVR()
        {
            enableVR = !enableVR;
            if (enableVR)
            {
                InitializeVR();
            }
            else
            {
                DisableVR();
            }
        }
        
        void DisableVR()
        {
            var xrSettings = XRGeneralSettings.Instance;
            if (xrSettings?.Manager?.activeLoader != null)
            {
                xrSettings.Manager.DeinitializeLoader();
                Debug.Log("VR disabled");
            }
            isVRInitialized = false;
        }
        
        void OnDestroy()
        {
            if (isVRInitialized)
            {
                DisableVR();
            }
        }
    }
}