import serial

# ⚠️ 반드시 idf.py monitor에서 확인한 포트 번호로 수정!
PORT = 'COM3' 
BAUD = 115200

try:
    ser = serial.Serial(PORT, BAUD, timeout=1)
    ser.flushInput()
    print(f"✅ {PORT} 연결 성공! 데이터를 기다립니다...")

    while True:
        if ser.in_waiting > 0:
            # decode 에러를 무시하고 날것의 데이터를 그대로 출력
            raw_data = ser.readline()
            try:
                line = raw_data.decode('utf-8').strip()
                print(f"데이터 수신: {line}")
            except:
                # 글자가 깨져서 들어올 경우 대비
                print(f"Raw 데이터: {raw_data}")
                
except Exception as e:
    print(f"❌ 에러 발생: {e}")