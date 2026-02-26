using UnityEngine;
using UnityEngine.Networking;
using System.Collections;
using System;

public class DroneTelemetry : MonoBehaviour
{
    [Header("ESP32 Settings")]
    public string esp32Url = "http://192.168.1.25";

    [Header("Battery")]
    public float battery = 100f;
    public float drainRatePerSecond = 0.3f;
    public float lowBatteryThreshold = 20f;

    DroneController controller;
    Rigidbody rb;

    // Event tracking
    bool wasArmed = false;
    bool wasLanded = true;
    bool lowBatteryEventSent = false;

    void Start() {
        controller = GetComponent<DroneController>();
        rb = GetComponent<Rigidbody>();
        StartCoroutine(TelemetryLoop());
    }

    IEnumerator TelemetryLoop() {
        while (true) {
            yield return new WaitForSeconds(1f);

            bool armed = controller.IsArmed();
            float altitude = Mathf.Max(0f, transform.position.y);
            bool landed = altitude < 0.2f;

            // Drain battery only when armed
            if (armed && battery > 0) {
                battery -= drainRatePerSecond;
                battery = Mathf.Max(battery, 0f);
            }

            // Kill drone if battery dead
            if (battery <= 0) {
                controller.Disarm();
                SendEvent("Battery DEAD - drone disarmed");
            }

            // --- Event detection ---
            if (armed != wasArmed) {
                SendEvent(armed ? "ARMED" : "DISARMED");
                wasArmed = armed;
            }

            if (landed != wasLanded) {
                SendEvent(landed ? "LANDED" : "TAKEOFF");
                wasLanded = landed;
            }

            if (battery <= lowBatteryThreshold && !lowBatteryEventSent) {
                SendEvent("Battery LOW (<= 20%)");
                lowBatteryEventSent = true;
            }

            // --- Send telemetry ---
            TelemetryPayload payload = new TelemetryPayload {
                armed = armed,
                battery = (int)battery,
                altitude = Mathf.Round(altitude * 100f) / 100f,
                clientNowMs = DateTimeOffset.UtcNow.ToUnixTimeMilliseconds()
            };

            StartCoroutine(PostJson("/telemetry", JsonUtility.ToJson(payload)));
        }
    }

    void SendEvent(string message) {
        EventPayload payload = new EventPayload { message = message };
        StartCoroutine(PostJson("/event", JsonUtility.ToJson(payload)));
    }

    IEnumerator PostJson(string endpoint, string json) {
        byte[] bodyRaw = System.Text.Encoding.UTF8.GetBytes(json);
        using var req = new UnityWebRequest(esp32Url + endpoint, "POST");
        req.uploadHandler = new UploadHandlerRaw(bodyRaw);
        req.downloadHandler = new DownloadHandlerBuffer();
        req.SetRequestHeader("Content-Type", "application/json");
        yield return req.SendWebRequest();

        if (req.result != UnityWebRequest.Result.Success)
            Debug.LogWarning("Telemetry failed: " + req.error);
    }

    [Serializable] class TelemetryPayload {
        public bool armed;
        public int battery;
        public float altitude;
        public long clientNowMs;
    }

    [Serializable] class EventPayload {
        public string message;
    }
}