<?php
// Read the raw binary data sent in the HTTP POST request body
$data = file_get_contents("php://input");

if (!empty($data)) {
    // Target directory for saved photos
    $uploadDir = "captures/";

    // Ensure the directory exists
    if (!is_dir($uploadDir)) {
        mkdir($uploadDir, 0755, true);
    }

    // Generate a unique filename based on the current timestamp
    $filename = $uploadDir . "photo_" . time() . ".jpg";

    // Save the binary data to disk
    if (file_put_contents($filename, $data) !== false) {
        http_response_code(200);
        echo "Success: Photo saved as " . $filename;
    } else {
        http_response_code(500);
        echo "Error: Failed to save file.";
    }
} else {
    http_response_code(400);
    echo "Error: No data received.";
}
?>
