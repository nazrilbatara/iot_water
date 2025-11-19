<!DOCTYPE html>
<html>
<head>
    <title>Sistem Kontrol Air IoT</title>
    <style>
        body { font-family: Arial, sans-serif; }
        table { border-collapse: collapse; width: 80%; margin: 20px auto; }
        th, td { border: 1px solid #ddd; padding: 8px; text-align: left; }
        th { background-color: #f2f2f2; }
        .fill { background-color: #d4edda; color: #155724; }
        .drain { background-color: #f8d7da; color: #721c24; }
    </style>
</head>
<body>

    <h2 style="text-align: center;">Log Kejadian Kontrol Air Otomatis</h2>

    <?php
    $servername = "localhost";
    $username = "root";
    $password = ""; 
    $dbname = "iot_water_control";

    // Buat Koneksi
    $conn = new mysqli($servername, $username, $password, $dbname);
    
    // Cek Koneksi
    if ($conn->connect_error) {
        die("Koneksi gagal: " . $conn->connect_error);
    }

    // Ambil 50 data terbaru, diurutkan dari yang terbaru
    $sql = "SELECT id, event_type, sensor_value, timestamp FROM event_log ORDER BY timestamp DESC LIMIT 50";
    $result = $conn->query($sql);

    if ($result->num_rows > 0) {
        echo "<table>";
        echo "<tr><th>ID</th><th>Tipe Kejadian</th><th>Nilai Sensor</th><th>Waktu</th></tr>";
        
        // Output data dari setiap baris
        while($row = $result->fetch_assoc()) {
            $class = (strtolower($row["event_type"]) == 'fill') ? 'fill' : 'drain';
            $unit = (strtolower($row["event_type"]) == 'fill') ? ' ADC' : ' ppm';
            
            echo "<tr class='$class'>";
            echo "<td>" . $row["id"] . "</td>";
            echo "<td>" . $row["event_type"] . "</td>";
            echo "<td>" . $row["sensor_value"] . $unit . "</td>";
            echo "<td>" . $row["timestamp"] . "</td>";
            echo "</tr>";
        }
        echo "</table>";
    } else {
        echo "<p style='text-align: center;'>Database kosong, tunggu ESP32 mengirim data pertama.</p>";
    }

    $conn->close();
    ?>

</body>
</html>