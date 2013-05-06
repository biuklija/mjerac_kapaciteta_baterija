#include <LiquidCrystal.h>
#include <SdFat.h>

// konfiguracija pinova 
#define SD_CS 10
#define OK_TIPKA 8
#define MOSFET 19
#define PWM_BACKLIGHT 9
#define BACKLIGHT_HIGH 250
#define BACKLIGHT_LOW 5
#define NAPON_BAT A0
#define NAPON_SHUNT A1
#define OTPOR_SHUNT 2.161 

#define CCHARS 0 // prikaz stanja baterije kod praznjenja
#define JOULI_D 1 // izracun joulea/wattsati
#define TRIM 1 // kraci stringovi
#define SD_CARD 0

// 0 - NiMh/Cd, 1 - Alkaline, 2 - LiIon, 3 - sve ostalo
byte vrstaBaterije = 3; 
// 0 - nema baterije, 1 - mjerenje u tijeku, 2 - mjerenje dovrseno
byte stanjeMjerenja = 0; 
const float cutoff[3] = {0.8, 0.3, 3.0}; // cutoff naponi u voltima, po vrstama baterije
float napon[2];
float kapacitet = 0;
#if JOULI_D
float jouli = 0;
#endif
#if !TRIM
static const char *vrstaBaterijeNaziv[] = {"Ni-MH/Cd", "Alkaline", "Li-Ion  "};
#endif
#if TRIM
static const char *vrstaBaterijeNaziv[] = {"Ni-MH", "Alkal", "LiIon"};
#endif
unsigned long pocetakMjerenja;
unsigned long zadnjeMjerenje; 
unsigned long krajMjerenja; 
unsigned long zadnjiPrikaz; // automatski izmjenjujemo sadrzaj displeja na kraju mjerenja
byte ekran=0; // nakon mjerenja rezultate prikazujemo u tri dijela

#if SD_CARD
const int chipSelect = 10; 
bool sdlog; 
SdFat sd;
SdFile logfile;
char filename[] = "L00.CSV"; // naziv datoteke
#endif 

LiquidCrystal lcd(2, 3, 4, 5, 6, 7); // konfiguracija LCD pinova


void setup()
{
        lcdCustomChars();
        lcd.begin(16, 2);
        pinMode(10, OUTPUT); // bez ovoga brejka za SD
//        digitalWrite(SD_CS, LOW);
        pinMode(PWM_BACKLIGHT, OUTPUT);
	analogWrite(PWM_BACKLIGHT, BACKLIGHT_HIGH); // backlight
	pinMode(NAPON_BAT, INPUT); // ispred shunta
	pinMode(NAPON_SHUNT, INPUT); // iza shunta
	pinMode(OK_TIPKA, INPUT_PULLUP); // tipkalo
	pinMode(MOSFET, OUTPUT); // gate mosfeta
	// prepoznavanje baterije
#if SD_CARD
        if (!sd.begin(chipSelect, SPI_HALF_SPEED)) 
        {
          sdlog = false;
        }
        else
        {
	sdlog = true;
        }
#endif		
	while(digitalRead(OK_TIPKA) == LOW)
	{
		mjerenje();
		while (napon[0] <= 0.3 )
		{
			lcd.home();
			lcd.print(F("Spojite bateriju"));
			mjerenje();
		}
		if (napon[0] >= 1.7) { vrstaBaterije = 2; }
		else if (napon[0] >= 1.5) { vrstaBaterije = 1; }
		else if (napon[0] >= 1.1) { vrstaBaterije = 0; }
		else 
		{ 
			lcd.print(F("Prazna baterija "));
			return;
		}
		lcd.home();
		lcd.print(F("Napon: "));
		lcd.print(napon[0]);
		lcd.print(F("V SD"));
		#if SD_CARD
		if (sdlog) // ispisi je li SD kartica pronadjena
		{
		    lcd.write((byte)6); // kvacica
		}
		else
		{
		    lcd.write((byte)7); // krzic
		}
		#endif
		lcd.setCursor(0,1);
		lcd.print(F("Tip:"));
		lcd.print(vrstaBaterijeNaziv[vrstaBaterije]);
		lcd.setCursor(13,1);
		lcd.print(F("OK?"));
		delay(500);
	}
	digitalWrite(MOSFET, HIGH); // zapocni mjerenje
	stanjeMjerenja = 1; 
#if SD_CARD
	if (sdlog) 
	{
		otvoriDatoteku();
	}
#endif
	pocetakMjerenja = millis();
	lcd.clear();
	analogWrite(PWM_BACKLIGHT, BACKLIGHT_LOW); // backlight
}

void loop()
{
	int brojMjerenja = 0;
	float prosjecniNapon[2] = {0, 0};
	switch(stanjeMjerenja) {
	case 1:
		while( napon[0] >= cutoff[vrstaBaterije])
		{
			brojMjerenja++;
			prosjecniNapon[0] += analogRead(A0) * 0.00488;
			prosjecniNapon[1] +=  analogRead(A1) * 0.00488;
			if (millis() >= zadnjeMjerenje + 1000)
			{
				napon[0] = prosjecniNapon[0] / brojMjerenja;
				napon[1] = prosjecniNapon[1] / brojMjerenja;
				prikazMjerenja();
				zadnjeMjerenje = millis(); 
				prosjecniNapon[0] = 0;
				prosjecniNapon[1] = 0;
				brojMjerenja = 0;
			}
		}
		stanjeMjerenja = 2;
		krajMjerenja = millis();
		digitalWrite(MOSFET, LOW);
#if SD_CARD
		if (sdlog) 
		{
			logfile.close();
		}
#endif
		analogWrite(PWM_BACKLIGHT, BACKLIGHT_HIGH); // backlight
		break; 
	case 2:
		if (millis() >= zadnjiPrikaz + 5000)
		{
			switch(ekran)	{
			case 0: 
				lcd.clear();
				lcd.print(F("Vrijeme"));
				lcd.setCursor(8,0);
				ispisiTrajanje(krajMjerenja);
				lcd.setCursor(0,1);
				lcd.print(F("mAh"));
				lcd.setCursor(8,1);
				lcd.print(kapacitet);
				zadnjiPrikaz = millis();
				#if JOULI_D
				ekran = 1; 
				#endif
				
				#if !JOULI_D
				ekran = 2; 
				#endif 
				break;
				#if JOULI_D
			case 1: 
				lcd.clear();
				lcd.print(F("Wattsati"));
				lcd.setCursor(9,0);
				lcd.print((float)jouli / 3600);
				lcd.setCursor(0,1);
				lcd.print(F("Joula"));
				lcd.setCursor(9,1);
				lcd.print((float)jouli / 1000);
				lcd.write(75);//K 
				zadnjiPrikaz = millis();
				ekran = 2; 
				break; 
				#endif
			case 2:
				lcd.clear();
				lcd.print(F("Kraj na: "));
				lcd.print(napon[0]);
				lcd.write(86); // V
#if SD_CARD
				if (sdlog)
				{
					lcd.setCursor(0,1);
					lcd.print(F("Log: "));
					lcd.print(filename);
				}
#endif 
				zadnjiPrikaz = millis();
				ekran = 0; 
				break; 		
			}
		}
		break; 
	}
}

#if CCHARS
byte prikazBaterije(float naponF)
{
	int napon = naponF * 1000;
	const unsigned int batstatus[3] = {900, 900, 3400}; // minimalni naponi za "praznu" bateriju 
	const byte korak[3] = {50, 100, 50}; // s ovim brojem dijelis razliku od batstatus do trenutnog napona
	byte stanje = (napon - batstatus[vrstaBaterije]) / korak[vrstaBaterije]; 
	if (stanje >= 5)
	{
		return(5); // umjesto return moze ovdje i ispis na LCD
	}
	else
	{
		return(stanje); 
	}

}
#endif 

void mjerenje()
{
	napon[0] = analogRead(A0) * 0.00488;
	napon[1] = analogRead(A1) * 0.00488;
}

void prikazMjerenja()
{
	lcd.home();
	ispisiTrajanje(millis());
#if CCHARS
	lcd.setCursor(9,0);
	lcd.write((byte)prikazBaterije(napon[0])); // stanje baterije - grafika
#endif
	lcd.setCursor(11,0);
	lcd.print(napon[0]);
	lcd.write(86); // V
	lcd.setCursor(0,1);
	float struja = (napon[0] - napon[1])/OTPOR_SHUNT;  
	kapacitet += (struja * 1000)/3600; // mAh  
	#if JOULI_D
	jouli += napon[0] * struja;
	#endif
	lcd.print(int(struja*1000));
	lcd.print(F("mA ")); 
	lcd.print(kapacitet); 
	lcd.print(F("mAh")); 
#if SD_CARD
	if (sdlog) {
		logfile.print((millis() - pocetakMjerenja)/1000);
		logfile.write(9);
		logfile.print(napon[0]); // - napon[1]
		logfile.write(9);
		logfile.print(struja);
		logfile.write(9);
		logfile.print(kapacitet);
		#if JOULI_D
		logfile.write(9);
		logfile.print(jouli);
		#endif
		logfile.print(F("\n"));
	} 
#endif 
}

void pocNula(int vrijednost)
{
	if (vrijednost <= 9)
	{
		lcd.write(48); // nula
		lcd.print(vrijednost);
	}
	else
	{
		lcd.print(vrijednost);
	}
}

void ispisiTrajanje(unsigned long vrijeme)
{
	int h, m, s; 
	unsigned long traje = vrijeme - pocetakMjerenja; 
	unsigned long over;
	h=int(traje/3600000);
	over=traje%3600000;
	m=int(over/60000);
	over=over%60000;
	s=int(over/1000);
	pocNula(h);
	lcd.write(58); // dvotocje
	pocNula(m);
	lcd.write(58);
	pocNula(s);
}

#if SD_CARD
void otvoriDatoteku(void)
{
	// create a new file
	for (uint8_t i = 0; i < 100; i++) {
		filename[1] = i/10 + '0';
		filename[2] = i%10 + '0';
		if (! sd.exists(filename)) {
			// only open a new file if it doesn't exist
			logfile.open(filename, O_RDWR | O_CREAT); 
			break;  		
		}
	}
	#if JOULI_D
	logfile.print(F("sec\tUbat\tI\tmAh\tJ\n")); // zaglavlje tablice
	#endif
	#if !JOULI_D
	logfile.print(F("sec\tUbat\tI\tmAh\n")); // zaglavlje tablice
	#endif
}
#endif 

void lcdCustomChars(void)
{
#if CCHARS
	const byte bat[64]={
		0B01110, 0B11011, 0B10001, 0B10001, 0B10001, 0B10001, 0B10001, 0B11111, // prazna baterija - 0
		0B01110, 0B11011, 0B10001, 0B10001, 0B10001, 0B10001, 0B11111, 0B11111, // 1
		0B01110, 0B11011, 0B10001, 0B10001, 0B10001, 0B11111, 0B11111, 0B11111, // 2
		0B01110, 0B11011, 0B10001, 0B10001, 0B11111, 0B11111, 0B11111, 0B11111,// 3
		0B01110, 0B11011, 0B10001, 0B11111, 0B11111, 0B11111, 0B11111, 0B11111, // 4
		0B01110, 0B11111, 0B11111, 0B11111, 0B11111, 0B11111, 0B11111, 0B11111,// 5
		0B00000, 0B00001, 0B00011, 0B10110, 0B11100, 0B01000, 0B00000, 0B00000, // kvacica
		0B00000, 0B11011, 0B01110, 0B00100, 0B01110, 0B11011, 0B00000, 0B00000 	}; // krizic

	byte pozicija = 0;
	byte znak[8]; 
	for (byte i=0; i < 8; i++)
	{
		for (byte j=0; j < 8; j++)
		{
			znak[j] = bat[pozicija]; // na mjesto "j" varijable adresa ucitaj bit
			pozicija++; // sljedeci bit 
		}
		lcd.createChar(i, znak);
		delay(50);
	}
	
#endif 
#if !CCHARS
	byte kvacica[8] = { 0B00000, 0B00001, 0B00011, 0B10110, 0B11100, 0B01000, 0B00000, 0B00000 }; // 6 kvacica
	byte krizic[8] = { 0B00000, 0B11011, 0B01110, 0B00100, 0B01110, 0B11011, 0B00000, 0B00000 }; // 7 krizic
	lcd.createChar(6, kvacica);
	lcd.createChar(7, krizic);
#endif
}
