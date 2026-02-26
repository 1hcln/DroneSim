using UnityEngine;

public class DroneController : MonoBehaviour
{
    Rigidbody rb;

    public float thrustForce = 15f;
    public float moveForce = 8f;
    public float rotateSpeed = 80f;
    public float hoverForce = 9.81f;

    bool isArmed = false;

    void Start() {
        rb = GetComponent<Rigidbody>();
        rb.linearDamping = 2f;
        rb.angularDamping = 4f;
    }

    void FixedUpdate() {
        // Arm with Space
        if (Input.GetKey(KeyCode.Space)) {
            isArmed = true;
        }

        if (!isArmed) return;

        // Hover — counteract gravity
        rb.AddForce(Vector3.up * hoverForce, ForceMode.Acceleration);

        // Move up/down — W/S
        if (Input.GetKey(KeyCode.W))
            rb.AddForce(Vector3.up * thrustForce, ForceMode.Acceleration);
        if (Input.GetKey(KeyCode.S))
            rb.AddForce(Vector3.down * thrustForce * 0.5f, ForceMode.Acceleration);

        // Left/right — A/D or arrow keys
        if (Input.GetKey(KeyCode.A) || Input.GetKey(KeyCode.LeftArrow))
            rb.AddForce(-transform.right * moveForce, ForceMode.Acceleration);
        if (Input.GetKey(KeyCode.D) || Input.GetKey(KeyCode.RightArrow))
            rb.AddForce(transform.right * moveForce, ForceMode.Acceleration);

        // Forward/back — arrow keys
        if (Input.GetKey(KeyCode.UpArrow))
            rb.AddForce(Vector3.forward * moveForce, ForceMode.Acceleration);
        if (Input.GetKey(KeyCode.DownArrow))
            rb.AddForce(Vector3.back * moveForce, ForceMode.Acceleration);

        // Rotate — Q/E
        if (Input.GetKey(KeyCode.Q))
            transform.Rotate(Vector3.up, -rotateSpeed * Time.fixedDeltaTime);
        if (Input.GetKey(KeyCode.E))
            transform.Rotate(Vector3.up, rotateSpeed * Time.fixedDeltaTime);
    }

    public bool IsArmed() => isArmed;
    public void Disarm() => isArmed = false;
}