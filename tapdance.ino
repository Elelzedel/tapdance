#define TCK_PIN D2
#define TMS_PIN D3
#define TDI_PIN D4
#define TDO_PIN D5
#define TRST_PIN D6

#define TCK_ALTERNATION_DELAY 1 // microseconds

#define START_IR 0x20000000
#define INCREMENT_IR 0x00000001

#define SKIP_IRS_MASK 0xF0000000

#define EXPECT_LEN 1

#define RESET_TAP_FREQ 10000
#define STATS_FREQ 100000

const int SKIP_IRS [] = {
	0x00000000, // EXTEST
	0x10000000, // SAMPLE/PRELOAD
	0x4FFFFFFF, // IDCODE
	0x60000000, // CLAMP
	0x70000000, // HIGHZ
	//0xF0000000FFFFFF  // BYPASS
};

int currentIR = START_IR;

// Reset on the first loop
int cyclesSinceTAPReset = RESET_TAP_FREQ;
int cyclesSinceStats = 0;

unsigned long timerStart = 0;

unsigned long tckLowStart = micros();

void setup() {
	pinMode(TCK_PIN, OUTPUT);
	pinMode(TMS_PIN, OUTPUT);
	pinMode(TDI_PIN, OUTPUT);
	pinMode(TDO_PIN, INPUT);
	pinMode(TRST_PIN, OUTPUT);

	// Configure MCO output to drive target
	pinMode(D7, GPIO_AF0_MCO);
	HAL_RCC_MCOConfig(RCC_MCO, RCC_MCO1SOURCE_SYSCLK, RCC_MCODIV_4);

	// Start by holding TAP(s) in reset
	digitalWrite(TRST_PIN, LOW);

	Serial.begin(9600);
	infoAndWait();

}

void loop() {
	if (cyclesSinceTAPReset >= RESET_TAP_FREQ) {
		resetTAP();
	} else {
		cyclesSinceTAPReset++;
	}

	if (cyclesSinceStats >= STATS_FREQ) {
		stats();
	} else {
		cyclesSinceStats++;
	}

	bool skip = false;
	for (int skipIR : SKIP_IRS) {
		// All that needs to match are the masked bits
		if ((currentIR & SKIP_IRS_MASK) == (skipIR & SKIP_IRS_MASK)) {
			skip = true;
			break;
		}
	}

	if (skip == false) {
		setIR(currentIR);
		int len = lenDR();
		if (len != EXPECT_LEN) {
			Serial.println("Interesting register identified");
			Serial.print("Current IR: 0x");
			Serial.println(currentIR, HEX);
			Serial.print("DR len: ");
			Serial.println(len);
			Serial.println("");
			// Printing this information resulted in a huge TCLK gap, reset TAP to be safe
			resetTAP();
		}
	} else {
		// Loop back to Select-DR to keep timing consistent in case of many consecutive skips
		tckPulse(LOW, HIGH);
		tckPulse(LOW, HIGH);
		tckPulse(LOW, LOW);
	}

	if (currentIR == 0xFFFFFFFF) {
		Serial.println("All combinations ran");
		halt();
	}

	currentIR = currentIR + INCREMENT_IR;
}

void stats() {
	cyclesSinceStats = 0;

	if (timerStart == 0) {
		timerStart = millis();
		return;
	}

	unsigned long timeElapsed = millis() - timerStart;
	timerStart = millis();

	Serial.print(STATS_FREQ);
	Serial.print(" IRs in ");
	Serial.print(float(timeElapsed)/1000.0);
	Serial.print("s");
	Serial.print(" - 0x");
	Serial.print(currentIR, HEX);
	Serial.print("/0x");
	Serial.println(0xFFFFFFFF, HEX);

	// Printing stats info is slow, reset TAP to be safe
	resetTAP();
}

void resetTAP() {
	cyclesSinceTAPReset = 0;

	// Reset TAP(s)
	digitalWrite(TRST_PIN, LOW);

	for (int i = 0; i < 100; i++) {
		tckPulse(LOW, LOW);
	}

	// Activate TAP(s)
	digitalWrite(TRST_PIN, HIGH);

	// Move to Test-Logic-Reset then hold Idle for 101 clocks
	for (int i = 0; i < 1001; i++) {
		tckPulse(LOW, LOW);
	}

	// Move to Select-DR-Scan
	tckPulse(LOW, HIGH);

	// Retest target by confirming IDCODE length is correct
	setIR(0x4FFFFFFF);
	if (lenDR() != 32) {
		Serial.println("Invalid IDCODE after reset, execution halted");
		Serial.print("Current IR: 0x");
		Serial.print(currentIR, HEX);
		Serial.println("");
		halt();
	}
}

void infoAndWait() {
	Serial.println("TapDance v0.1");
	Serial.println("");
	Serial.println("Skipping IRs:");

	for (int skipIR : SKIP_IRS) {
		Serial.print("> 0x");
		Serial.println(skipIR, HEX);
	}
	Serial.println("");

	Serial.print("Using IR mask: 0x");
	Serial.println(SKIP_IRS_MASK, HEX);
	Serial.println("");

	Serial.print("TCK freq: ");
	float tckPeriodMicroseconds = float(TCK_ALTERNATION_DELAY) * 2.0;
	float tckHz = 1000000.0/tckPeriodMicroseconds;
	float tckkHz = tckHz/1000.0;
	Serial.print(tckkHz);
	Serial.println(" kHz");
	Serial.println("");

	Serial.println("System ready, press any key to start...");
	while(!Serial.available()) { Serial.read(); }
	Serial.println("Starting...");
}

// Assumes we are currently at Select-DR-Scan
void setIR(int ir) {
	// Move to Select-IR-Scan
	tckPulse(LOW, HIGH);
	// Move to Capture-IR
	tckPulse(LOW, LOW);
	// Move to Shift-IR
	tckPulse(LOW, LOW);

	for (int i = 0; i < 32; i++)  {
		// If we're writing the last bit, transition to Exit1-IR
		uint8_t tms = LOW;
		if (i == 31) {
			tms = HIGH;
		}

		int nthBit = (ir >> i) & 1;
		if (nthBit == 1) {
			tckPulse(HIGH, tms);
		} else {
			tckPulse(LOW, tms);
		}
	}

	// Currently at Exit1-IR, move to Update-IR
	tckPulse(LOW, HIGH);
	// Move to Select-DR-Scan
	tckPulse(LOW, HIGH);
}

// Assumes we are currently at Select-DR-Scan
int lenDR() {
	// Move to Capture-DR
	tckPulse(LOW, LOW);
	// Move to Shift-DR
	tckPulse(LOW, LOW);

	// 0 out the register (unless it's longer than 64 bits)
	for (int i = 0; i < 64; i++) {
		tckPulse(LOW, LOW);
	}

	// Clock 1 into the register and see how long it takes to see it takes to reach the end
	tckPulse(HIGH, LOW);

	int len = 1;
	for (; len <= 64; len++) {
		uint8_t tdo = tckPulse(HIGH, LOW);

		if (tdo == HIGH) {
			break;
		}
	}

	// Move to Exit1-DR
	tckPulse(LOW, HIGH);
	// Move to Update-DR
	tckPulse(LOW, HIGH);
	// Move to Select-DR-Scan
	tckPulse(LOW, HIGH);

	return len;
}

uint8_t tckPulse(uint8_t tdi, uint8_t tms) {
	// We'll get the last TDO bit and then set TDO and TMS before we clock
	uint8_t tdo = digitalRead(TDO_PIN);
	digitalWrite(TDI_PIN, tdi);
	digitalWrite(TMS_PIN, tms);

	delayMicroseconds(TCK_ALTERNATION_DELAY);
	digitalWrite(TCK_PIN, HIGH);

	delayMicroseconds(TCK_ALTERNATION_DELAY);
	digitalWrite(TCK_PIN, LOW);
	return tdo;
}

void halt() {
	while(true) {
		delay(10000);
	}
}
