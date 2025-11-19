<?php
$servername = "localhost";
$username = "root";
$password = ""; 
$dbname = "iot_water_control";

// Periksa apakah data POST diterima
if (isset($_POST["event_type"]) && isset($_POST["sensor_value"])) {
    $event_type = $_POST["event_type"];
    $sensor_value = $_POST["sensor_value"];

    // Buat Koneksi
    $conn = new mysqli($servername, $username, $password, $dbname);
    
    // Cek Koneksi
    if ($conn->connect_error) {
        die("Koneksi gagal: " . $conn->connect_error);
    }

    // Query SQL untuk memasukkan data
    $sql = "INSERT INTO event_log (event_type, sensor_value) VALUES (?, ?)";
    
    // Persiapan Statement (untuk keamanan dari SQL Injection)
    $stmt = $conn->prepare($sql);
    $stmt->bind_param("sd", $event_type, $sensor_value); // s: string, d: double

    if ($stmt->execute()) {
        echo "Data berhasil disimpan."; // Response untuk ESP32 (opsional)
    } else {
        echo "Error: " . $sql . "<br>" . $conn->error;
    }

    $stmt->close();
    $conn->close();
} else {
    echo "Tidak ada data yang diterima dari ESP32.";
}

?>
