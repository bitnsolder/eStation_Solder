#include "pid.h"
#include "qlog.h"

/* Initialize */
void PID_Init(PID_TypeDef *uPID)
{
	/* Set parameters */
	uPID->OutputSum = *uPID->MyOutput;
	uPID->LastInput = *uPID->MyInput;

	uPID->OutputSum = float_clamp(uPID->OutputSum, uPID->OutMin, uPID->OutMax);
}

void PID(PID_TypeDef *uPID, volatile float *Input, volatile float *Output, volatile float *Setpoint, float Kp, float Ki, float Kd, PIDCD_TypeDef ControllerDirection)
{
	/* Set parameters */
	uPID->MyOutput   = Output;
	uPID->MyInput    = Input;
	uPID->MySetpoint = Setpoint;
	uPID->InAuto     = (PIDMode_TypeDef)0;

	PID_SetOutputLimits(uPID, 0, DEFAULT_PWM_MAX);
	uPID->SampleTime = DEFAULT_SAMPLE_TIME_MS;

	PID_SetControllerDirection(uPID, ControllerDirection);
	PID_SetTunings(uPID, Kp, Ki, Kd);

	uPID->LastTime = HAL_GetTick() - uPID->SampleTime;
}

/* Function to clamp d between the limits min and max */
float float_clamp(float d, float min, float max) {
	  const float t = d < min ? min : d;
	  return t > max ? max : t;
}

/* Function to check if clamping will occur */
uint8_t check_clamping(float d, float min, float max) {
	  if(d > max || d < min){
		  return 1;
	  }
	  else{
		  return 0;
	  }
}

/* Compute  */
/* Global hoặc Static để giữ mốc */
static uint32_t start_heating_time = 0;
static uint32_t last_run_tick = 0;

uint8_t PID_Compute_Advanced(PID_TypeDef *uPID) {
    uint32_t now = HAL_GetTick();

    // Đánh dấu mốc khi bắt đầu Heating
    if (start_heating_time == 0) {
        start_heating_time = now;
        last_run_tick = now;
    }

    // Tính dt thực tế (giây) thay vì dùng hằng số 0.01f
    // Điều này giúp PID chính xác ngay cả khi log làm CPU chạy chậm lại
    float dt_actual = (float)(now - last_run_tick) / 1000.0f;
    if (dt_actual <= 0) dt_actual = 0.001f; // Tránh chia cho 0
    last_run_tick = now;

    float input = *uPID->MyInput;
    float set = *uPID->MySetpoint;
    float error = set - input;

    // 1. Vi phân dùng dt thực tế
    float dInput = (input - uPID->LastInput) / dt_actual;
    static float dInput_f = 0;
    dInput_f = 0.8f * dInput_f + 0.2f * dInput;

    float out_final = 0;

    // --- Giữ nguyên logic Hybrid của mày ---
    if (error > 10.0f) {
        out_final = uPID->OutMax;
        //uPID->OutputSum = uPID->OutMax * 0.15f;
    }
    else if (error < 0.0f) {

        uPID->OutputSum *= 0.8f;
        if (error < -0.5f) {
          out_final = 0;
        } else {
          out_final *= 0.05f;
        }
    }
    else {
        float P = uPID->Kp * error;
        uPID->OutputSum += uPID->Ki * error * dt_actual; // Dùng dt thực

        if (uPID->OutputSum > uPID->OutMax * 0.7f) uPID->OutputSum = uPID->OutMax * 0.7f;
        if (uPID->OutputSum < 0) uPID->OutputSum = 0;

        float D = -uPID->Kd * dInput_f;
        float out_pid = P + uPID->OutputSum + D;

        if (error > 3.0f) {
            float weight = 1.0f - ((error - 3.0f) / (15.0f - 3.0f));
            out_final = (weight * out_pid) + ((1.0f - weight) * uPID->OutMax);
        } else {
            out_final = out_pid;
        }

        float predict = input + (dInput_f * 0.12f);
        if ((predict > set + 5)  && dInput_f > 0) {
            float brake = (predict - set) * 25.0f;
            out_final -= brake;
        }
    }

    if (out_final > uPID->OutMax) out_final = uPID->OutMax;
    if (out_final < 0) out_final = 0;

    *uPID->MyOutput = out_final;
    uPID->LastInput = input;

    // LOG CHUẨN: [Thời gian từ lúc bắt đầu][Sai số][Công suất][dt thực tế]
    QuickLog_Write("[%lu ms] e=%.2f, out=%.1f\r\n",
              now - start_heating_time, error, out_final);

    return 1;
}

//static uint32_t cnt = 0;
//uint8_t PID_Compute_Advanced(PID_TypeDef *uPID)
//{
//    const float dt = 0.01f;
//    if (!uPID->InAuto) return 0;
//
//    float input_raw  = *uPID->MyInput;
//    float set        = *uPID->MySetpoint;
//    float error      = set - input_raw;
//
//    static float last_temp = 0;
//    static float last_out  = 0;
//    static float dInput_f  = 0;
//    static uint8_t initialized = 0;
//
//    if (!initialized) {
//        last_temp = input_raw;
//        uPID->LastInput = input_raw;
//        initialized = 1;
//    }
//
//    // 1. Vi phân và Bộ lọc
//    float dInput = (input_raw - uPID->LastInput) / dt;
//    dInput_f = 0.85f * dInput_f + 0.15f * dInput;
//
//    float out = 0;
//
//    // =========================================================
//    // 2. STATE MACHINE 3 GIAI ĐOẠN (HẠ CÁNH MỀM)
//    // =========================================================
//    if (error > 20.0f)
//    {
//        // GIAI ĐOẠN 1: HEAT - Đốt mạnh
//        out = uPID->OutMax;
//        // Nạp sẵn nền I nhẹ (10% thay vì 15% để tránh vọt lố)
//        uPID->OutputSum = uPID->OutMax * 0.15f;
//    }
//    else if (error > 5.0f)
//    {
//        // GIAI ĐOẠN 2: APPROACH - Giảm tốc tuyến tính
//        // Giảm dần từ 95% về 40% để triệt tiêu đà tăng nhiệt
//        float ratio = (error - 5.0f) / (15.0f - 5.0f);
//        float max_p = uPID->OutMax;
//        float min_p = 0.60f * uPID->OutMax;
//        out = min_p + (max_p - min_p) * ratio;
//    }
//    else
//    {
//      float P = uPID->Kp * error;
//      float D = -uPID->Kd * dInput_f;
//
//      if (error > 0) { // Nếu đang thiếu nhiệt (như lúc bị rơi xuống 216)
//          // Tăng tốc khâu I lên gấp 2.5 lần để bù lại đoạn sụt nhanh hơn
//          uPID->OutputSum += (uPID->Ki * 10.0f) * error * dt;
//      } else {
//          // Nếu quá nhiệt (error < 0), xả I nhanh để tránh mốc 225
//          uPID->OutputSum += (uPID->Ki * 1.0f) * error * dt;
//      }
//
//      // Nâng trần Anti-windup lên một chút để khâu I có quyền lực hơn
//      if (uPID->OutputSum > uPID->OutMax * 0.75f) uPID->OutputSum = uPID->OutMax * 0.75f;
//
//      out = P + uPID->OutputSum + D;
//    }
//
//    // =========================================================
//    // 3. CƠ CHẾ PHANH CƯỠNG BỨC (CHỈ KÍCH HOẠT KHI SÁT ĐÍCH)
//    // =========================================================
//    float predict = input_raw + (dInput_f * 0.18f); // Tăng dự đoán lên chút để nhạy đà hơn
//
//    // CHỈ PHANH KHI:
//    // 1. Đã vào vùng PID (error < 5.0)
//    // 2. Dự báo sẽ vọt quá Setpoint
//    // 3. Nhiệt độ vẫn đang đi lên (dInput_f > 0)
//    if (error < 10.0f && predict > set && dInput_f > 0)
//    {
//        float excess = predict - set;
//
//        // Tỉ lệ phanh: Càng gần đích phanh càng được phép can thiệp sâu
//        // Khi error = 5.0, factor ~ 0.16 (phanh nhẹ)
//        // Khi error = 0.5, factor ~ 0.66 (phanh mạnh)
//        float brake_factor = 1.0f / (error + 1.0f);
//
//        // Tính lực giảm: Dựa trên mức độ vọt (excess) và tốc độ tăng (dInput_f)
//        float reduction = excess * brake_factor * (dInput_f * 0.15f);
//
//        // THAY ĐỔI QUAN TRỌNG:
//        // Thay vì dùng brake_floor cứng nhắc, ta giới hạn mức giảm tối đa
//        // không được quá 40% giá trị output hiện tại.
//        // Điều này ngăn việc output bị "giụt" về 0 hoặc về OutputSum quá đột ngột.
//        float max_reduction = out * 0.1f;
//        if (reduction > max_reduction) reduction = max_reduction;
//
//        out -= reduction;
//    }
//
//    // Nếu thực tế đã vọt quá 1 độ, ép xả I (Integral Leak)
//    if (error < -1.0f) {
//        uPID->OutputSum *= 0.95f;
//        if (error < -3.0f) out = 0; // Vọt quá 3 độ thì ngắt hẳn
//    }
//
//    // Slew limit để tránh sốc dòng
//    //if (out > last_out + 150.0f) out = last_out + 150.0f;
//
//    // =========================================================
//    // 4. APPLY
//    // =========================================================
//    if (out > uPID->OutMax) out = uPID->OutMax;
//    if (out < 0) out = 0;
//
//
//
//    *uPID->MyOutput = out;
//    uPID->LastInput = input_raw;
//    last_temp = input_raw;
//    last_out = out;
//
//    cnt++;
//    debug_log("[%u] e=%.3f,out=%.3f\r\n",cnt,error,out);
//    return 1;
//}
void PID_SetMode(PID_TypeDef *uPID, PIDMode_TypeDef Mode){
	uint8_t newAuto = (Mode == _PID_MODE_AUTOMATIC);

	/* Initialize the PID */
	if (newAuto && !uPID->InAuto){
		PID_Init(uPID);
	}
	uPID->InAuto = (PIDMode_TypeDef)newAuto;
}

PIDMode_TypeDef PID_GetMode(PID_TypeDef *uPID){
	return uPID->InAuto ? _PID_MODE_AUTOMATIC : _PID_MODE_MANUAL;
}

/* PID Limits */
void PID_SetOutputLimits(PID_TypeDef *uPID, float Min, float Max){
	/* Check value */
	if (Min >= Max){
		return;
	}

	uPID->OutMin = Min;
	uPID->OutMax = Max;

	if (uPID->InAuto){
		/* Check value */
		*uPID->MyOutput = float_clamp(*uPID->MyOutput, uPID->OutMin, uPID->OutMax);

		/* Check out value */
		uPID->OutputSum = float_clamp(uPID->OutputSum, uPID->OutMin, uPID->OutMax);
	}
}

/* PID I-windup Limits */
void PID_SetILimits(PID_TypeDef *uPID, float Min, float Max){
	/* Check value */
	if (Min >= Max){
		return;
	}

	uPID->IMin = Min;
	uPID->IMax = Max;
}

/* Minimum error where I is added */
void PID_SetIminError(PID_TypeDef *uPID, float IminError){	/* Check value */
	if (IminError < 0){
		return;
	}

	uPID->IminError = IminError;
}

/* Set the I gain multiplier for negative error*/
void PID_SetNegativeErrorIgainMult(PID_TypeDef *uPID, float NegativeErrorIgainMultiplier, float NegativeErrorIgainBias){
	if (NegativeErrorIgainMultiplier < 0){
		return;
	}

	uPID->NegativeErrorIgainMultiplier = NegativeErrorIgainMultiplier;
	uPID->NegativeErrorIgainBias = NegativeErrorIgainBias;
}

/* PID Tunings */
void PID_SetTunings(PID_TypeDef *uPID, float Kp, float Ki, float Kd){
	/* Check value */
	if (Kp < 0 || Ki < 0 || Kd < 0){
		return;
	}

	uPID->DispKp = Kp;
	uPID->DispKi = Ki;
	uPID->DispKd = Kd;

	uPID->Kp = Kp;
	uPID->Ki = Ki;
	uPID->Kd = Kd;

	/* Check direction */
	if (uPID->ControllerDirection == _PID_CD_REVERSE){

		uPID->Kp = (0 - uPID->Kp);
		uPID->Ki = (0 - uPID->Ki);
		uPID->Kd = (0 - uPID->Kd);
	}
}

/* PID Direction */
void PID_SetControllerDirection(PID_TypeDef *uPID, PIDCD_TypeDef Direction){
	/* Check parameters */
	if ((uPID->InAuto) && (Direction !=uPID->ControllerDirection)){
		uPID->Kp = (0 - uPID->Kp);
		uPID->Ki = (0 - uPID->Ki);
		uPID->Kd = (0 - uPID->Kd);
	}

	uPID->ControllerDirection = Direction;
}

PIDCD_TypeDef PID_GetDirection(PID_TypeDef *uPID){
	return uPID->ControllerDirection;
}

/* PID Sampling */
void PID_SetSampleTime(PID_TypeDef *uPID, int32_t NewSampleTime, int32_t updateOnCall){
	if(updateOnCall > 0){
		updateOnCall = 1;
	}
	uPID->updateOnEveryCall = updateOnCall;
	float ratio;

	/* Check value */
	if (NewSampleTime > 0){
		ratio = (float)NewSampleTime / (float)uPID->SampleTime;

		uPID->Ki *= ratio;
		uPID->Kd /= ratio;
		uPID->SampleTime = (uint32_t)NewSampleTime;
	}
}

/* Get Parameters */
float PID_GetKp(PID_TypeDef *uPID){
	return uPID->DispKp;
}
float PID_GetKi(PID_TypeDef *uPID){
	return uPID->DispKi;
}
float PID_GetKd(PID_TypeDef *uPID){
	return uPID->DispKd;
}

/* Get current contributions*/
float PID_GetPpart(PID_TypeDef *uPID){
	return uPID->DispKp_part;
}
float PID_GetIpart(PID_TypeDef *uPID){
	return uPID->DispKi_part;
}
float PID_GetDpart(PID_TypeDef *uPID){
	return uPID->DispKd_part;
}
