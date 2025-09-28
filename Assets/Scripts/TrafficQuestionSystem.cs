using UnityEngine;
using UnityEngine.UI;
using System.Collections.Generic;
using System;

[System.Serializable]
public class TrafficQuestion
{
    public string question;
    public string[] answers;
    public int correctAnswerIndex;
    public string explanation;
}

public class TrafficQuestionSystem : MonoBehaviour
{
    [Header("UI Elements")]
    public Canvas questionCanvas;
    public Text questionText;
    public Button[] answerButtons;
    public Text explanationText;
    public Button continueButton;
    
    [Header("Question Data")]
    public TrafficQuestion[] questions;
    
    private int currentQuestionIndex = -1;
    private VehicleMovement vehicleMovement;
    private bool isQuestionActive = false;
    
    void Start()
    {
        vehicleMovement = FindObjectOfType<VehicleMovement>();
        InitializeQuestions();
        HideQuestionUI();
    }
    
    void InitializeQuestions()
    {
        // Initialize with some basic traffic rules questions
        questions = new TrafficQuestion[]
        {
            new TrafficQuestion
            {
                question = "Что означает красный сигнал светофора?",
                answers = new string[] { "Можно ехать", "Стоп! Движение запрещено", "Приготовиться к движению", "Снизить скорость" },
                correctAnswerIndex = 1,
                explanation = "Красный сигнал светофора означает полную остановку движения."
            },
            new TrafficQuestion
            {
                question = "На каком расстоянии от пешеходного перехода нельзя парковаться?",
                answers = new string[] { "3 метра", "5 метров", "10 метров", "15 метров" },
                correctAnswerIndex = 1,
                explanation = "Парковка запрещена ближе 5 метров до пешеходного перехода."
            },
            new TrafficQuestion
            {
                question = "Что нужно делать при знаке 'Уступи дорогу'?",
                answers = new string[] { "Остановиться", "Снизить скорость", "Уступить дорогу", "Подать сигнал" },
                correctAnswerIndex = 2,
                explanation = "Знак 'Уступи дорогу' обязывает уступить дорогу транспорту на главной дороге."
            },
            new TrafficQuestion
            {
                question = "Максимальная скорость в городе для легкового автомобиля?",
                answers = new string[] { "40 км/ч", "50 км/ч", "60 км/ч", "70 км/ч" },
                correctAnswerIndex = 2,
                explanation = "В населенных пунктах максимальная скорость для легковых автомобилей - 60 км/ч."
            },
            new TrafficQuestion
            {
                question = "Что означает желтый сигнал светофора?",
                answers = new string[] { "Можно ехать", "Стоп", "Внимание, подготовка к смене сигнала", "Поворот разрешен" },
                correctAnswerIndex = 2,
                explanation = "Желтый сигнал предупреждает о скорой смене сигнала светофора."
            }
        };
    }
    
    public void ShowRandomQuestion()
    {
        if (isQuestionActive || questions.Length == 0) return;
        
        currentQuestionIndex = UnityEngine.Random.Range(0, questions.Length);
        ShowQuestion(currentQuestionIndex);
    }
    
    void ShowQuestion(int questionIndex)
    {
        isQuestionActive = true;
        questionCanvas.gameObject.SetActive(true);
        
        TrafficQuestion question = questions[questionIndex];
        questionText.text = question.question;
        
        // Setup answer buttons
        for (int i = 0; i < answerButtons.Length && i < question.answers.Length; i++)
        {
            answerButtons[i].gameObject.SetActive(true);
            answerButtons[i].GetComponentInChildren<Text>().text = question.answers[i];
            
            int answerIndex = i; // Capture for closure
            answerButtons[i].onClick.RemoveAllListeners();
            answerButtons[i].onClick.AddListener(() => OnAnswerSelected(answerIndex));
        }
        
        // Hide unused buttons
        for (int i = question.answers.Length; i < answerButtons.Length; i++)
        {
            answerButtons[i].gameObject.SetActive(false);
        }
        
        explanationText.gameObject.SetActive(false);
        continueButton.gameObject.SetActive(false);
    }
    
    public void OnAnswerSelected(int selectedAnswerIndex)
    {
        TrafficQuestion question = questions[currentQuestionIndex];
        bool isCorrect = selectedAnswerIndex == question.correctAnswerIndex;
        
        // Show result
        string resultText = isCorrect ? "Правильно! " : "Неправильно! ";
        explanationText.text = resultText + question.explanation;
        explanationText.gameObject.SetActive(true);
        
        // Color the buttons to show correct/incorrect
        for (int i = 0; i < answerButtons.Length; i++)
        {
            if (i == question.correctAnswerIndex)
            {
                answerButtons[i].image.color = Color.green;
            }
            else if (i == selectedAnswerIndex && !isCorrect)
            {
                answerButtons[i].image.color = Color.red;
            }
        }
        
        continueButton.gameObject.SetActive(true);
        continueButton.onClick.RemoveAllListeners();
        continueButton.onClick.AddListener(OnContinuePressed);
    }
    
    public void OnContinuePressed()
    {
        HideQuestionUI();
        
        // Resume vehicle movement
        if (vehicleMovement != null)
        {
            vehicleMovement.OnQuestionAnswered();
        }
    }
    
    void HideQuestionUI()
    {
        isQuestionActive = false;
        questionCanvas.gameObject.SetActive(false);
        
        // Reset button colors
        foreach (Button button in answerButtons)
        {
            button.image.color = Color.white;
        }
    }
}