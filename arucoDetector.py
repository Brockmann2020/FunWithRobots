import cv2
import cv2.aruco as aruco

# Kamera öffnen (0 = Standardkamera)
cap = cv2.VideoCapture(2) # Index für interne Webcam: 0 ; Index für externe Webcam: hier 2

# ArUco-Dictionary (hier: 4x4 Marker mit 50 möglichen IDs)
aruco_dict = aruco.getPredefinedDictionary(aruco.DICT_4X4_50)
parameters = aruco.DetectorParameters()

# Marker-Detektor initialisieren (ab OpenCV 4.7.0)
detector = aruco.ArucoDetector(aruco_dict, parameters)

print("Drücke 'q' zum Beenden.")

cap.set(cv2.CAP_PROP_FRAME_WIDTH, 1280)
cap.set(cv2.CAP_PROP_FRAME_HEIGHT, 960)

while True:
    ret, frame = cap.read()
    if not ret:
        print("Fehler beim Lesen des Kamerabildes.")
        break

    # Marker erkennen
    corners, ids, _ = detector.detectMarkers(frame)

    # Marker anzeigen
    if ids is not None:
        frame = aruco.drawDetectedMarkers(frame, corners, ids)
        for i, corner in zip(ids, corners):
            print(f"Erkannter Marker ID: {i[0]}")

    # Bild anzeigen
    cv2.imshow('ArUco Marker Detection', frame)

    # Beenden mit 'q'
    if cv2.waitKey(1) & 0xFF == ord('q'):
        break

cap.release()
cv2.destroyAllWindows()