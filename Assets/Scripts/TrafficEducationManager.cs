using UnityEngine;
using UnityEngine.UI;

namespace VRTrafficEdu
{
    /// <summary>
    /// Main manager for the Traffic Education VR Demo
    /// Coordinates all systems and manages the overall experience
    /// </summary>
    public class TrafficEducationManager : MonoBehaviour
    {
        [Header("System References")]
        public VRSetupManager vrSetupManager;
        public VRPlayerController playerController;
        public VehicleMovement vehicle;
        public TrafficQuestionSystem questionSystem;
        public CityGenerator cityGenerator;
        
        [Header("UI Elements")]
        public Canvas mainUICanvas;
        public Text statusText;
        public Button startButton;
        public Button resetButton;
        public Button toggleVRButton;
        
        [Header("Demo Settings")]
        public bool autoStartDemo = true;
        public float demoStartDelay = 3f;
        
        private bool isDemoRunning = false;
        private float startTime;
        
        void Start()
        {
            InitializeDemo();
        }
        
        void InitializeDemo()
        {
            startTime = Time.time;
            
            // Find components if not assigned
            if (vrSetupManager == null)
                vrSetupManager = FindObjectOfType<VRSetupManager>();
            if (playerController == null)
                playerController = FindObjectOfType<VRPlayerController>();
            if (vehicle == null)
                vehicle = FindObjectOfType<VehicleMovement>();
            if (questionSystem == null)
                questionSystem = FindObjectOfType<TrafficQuestionSystem>();
            if (cityGenerator == null)
                cityGenerator = FindObjectOfType<CityGenerator>();
            
            SetupUI();
            
            if (autoStartDemo)
            {
                Invoke(nameof(StartDemo), demoStartDelay);
            }
            
            UpdateStatus("Демо VR обучения ПДД инициализировано");
        }
        
        void SetupUI()
        {
            if (startButton != null)
                startButton.onClick.AddListener(StartDemo);
            if (resetButton != null)
                resetButton.onClick.AddListener(ResetDemo);
            if (toggleVRButton != null)
                toggleVRButton.onClick.AddListener(ToggleVR);
        }
        
        void Update()
        {
            if (isDemoRunning)
            {
                UpdateDemoStatus();
            }
            
            // Handle input for non-VR mode
            if (Input.GetKeyDown(KeyCode.R))
            {
                ResetDemo();
            }
            
            if (Input.GetKeyDown(KeyCode.V))
            {
                ToggleVR();
            }
            
            if (Input.GetKeyDown(KeyCode.Space))
            {
                if (isDemoRunning)
                    PauseDemo();
                else
                    StartDemo();
            }
        }
        
        public void StartDemo()
        {
            if (isDemoRunning)
            {
                UpdateStatus("Демо уже запущено");
                return;
            }
            
            isDemoRunning = true;
            
            // Start vehicle movement
            if (vehicle != null)
            {
                vehicle.ResumeVehicle();
            }
            
            // Enable question system
            if (questionSystem != null)
            {
                // Question system is automatically triggered by vehicle movement
            }
            
            // Setup player following
            if (playerController != null)
            {
                playerController.SetFollowVehicle(true);
            }
            
            UpdateStatus("Демо запущено! Наблюдайте за движением автомобиля и отвечайте на вопросы по ПДД");
            
            if (startButton != null)
                startButton.interactable = false;
        }
        
        public void PauseDemo()
        {
            if (!isDemoRunning) return;
            
            isDemoRunning = false;
            
            if (vehicle != null)
            {
                vehicle.StopVehicle();
            }
            
            UpdateStatus("Демо приостановлено");
            
            if (startButton != null)
                startButton.interactable = true;
        }
        
        public void ResetDemo()
        {
            isDemoRunning = false;
            
            // Reset vehicle position
            if (vehicle != null && vehicle.waypoints.Length > 0)
            {
                vehicle.transform.position = vehicle.waypoints[0].position;
                vehicle.StopVehicle();
            }
            
            // Reset player position
            if (playerController != null)
            {
                playerController.SetFollowVehicle(true);
            }
            
            UpdateStatus("Демо сброшено. Нажмите 'Старт' для начала");
            
            if (startButton != null)
                startButton.interactable = true;
        }
        
        public void ToggleVR()
        {
            if (vrSetupManager != null)
            {
                vrSetupManager.ToggleVR();
                
                string vrStatus = vrSetupManager.IsVRActive() ? "включен" : "выключен";
                UpdateStatus($"VR режим {vrStatus}");
            }
        }
        
        void UpdateDemoStatus()
        {
            float demoTime = Time.time - startTime;
            
            if (statusText != null && demoTime > 5f) // Update status periodically
            {
                string vrMode = vrSetupManager != null && vrSetupManager.IsVRActive() ? "VR" : "Desktop";
                string speed = vehicle != null ? vehicle.speed.ToString("F1") : "0";
                
                string status = $"Режим: {vrMode} | Скорость: {speed} км/ч | Время: {demoTime:F0}с";
                statusText.text = status;
            }
        }
        
        void UpdateStatus(string message)
        {
            Debug.Log($"[TrafficEdu] {message}");
            
            if (statusText != null)
            {
                statusText.text = message;
            }
        }
        
        // Public methods for external control
        public void SetVehicleSpeed(float speed)
        {
            if (vehicle != null)
            {
                vehicle.SetSpeed(speed);
                UpdateStatus($"Скорость изменена: {speed} км/ч");
            }
        }
        
        public void TeleportPlayerToVehicle()
        {
            if (playerController != null && vehicle != null)
            {
                Vector3 targetPos = vehicle.transform.position + new Vector3(0, 2, -5);
                playerController.TeleportTo(targetPos);
                UpdateStatus("Игрок телепортирован к автомобилю");
            }
        }
        
        // Event handlers for question system
        public void OnQuestionShown()
        {
            UpdateStatus("Отвечайте на вопрос по ПДД");
        }
        
        public void OnQuestionAnswered(bool correct)
        {
            string result = correct ? "Правильно!" : "Неправильно!";
            UpdateStatus($"{result} Автомобиль продолжит движение");
        }
        
        void OnApplicationPause(bool pauseStatus)
        {
            if (pauseStatus && isDemoRunning)
            {
                PauseDemo();
            }
        }
    }
}