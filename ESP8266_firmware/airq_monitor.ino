/* Versione Beta 
   gennaio 2022
*/

#include <SFE_BMP180.h>       
#include <Wire.h>             /* I2C */
#include <ESP8266WiFiMulti.h> /* protocollo wifi */
#include <DHT.h>              /* libreria DHT  */
#include <InfluxDbClient.h>   /* libreria di comunicazione con InfluxDB */
#include <InfluxDbCloud.h>    /* gestione del token di sicurezza */
#include "sps30.h"            /* Sensirion SPS-30 sensore di particolato 

NOTA: La libreria utilizzata per SPS30 è quella di Paul van Haastrecht (https://github.com/paulvha/sps30)
in quanto è al momento l'unica che implementa la comunicazione via seriale
*/

/* costanti per le connessioni */

#define WIFI_SSID       "<-- Your Wi-Fi SSID -->"
#define WIFI_PASSWORD   "<-- Your Wi-Fi Password -->"

/*  https://docs.influxdata.com/influxdb/cloud/reference/regions/ */
#define INFLUXDB_URL    "<-- Influx DB URL -->"      
#define INFLUXDB_ORG    "<-- Your Organization -->"

/* Configurazione API */
#define INFLUXDB_BUCKET "<-- Your Bucket -->"
#define INFLUXDB_TOKEN  "<-- Your Token  -->"

/* Costanti per l'accesso a DH11 */
#define DHTPIN  D6
#define DHTTYPE DHT11

/* Altitudine SLM (in metri) della località */
#define ALTITUDE 172.0 

/* Tempo di Deep Sleep in usec */
#define SLEEP_TIME 30e6

/* SPS30 serial comm */
#define SP30_COMMS SERIALPORT
#define TX_PIN 0
#define RX_PIN 0

/* SPS 30
 * define driver debug
 * 0 : no messages
 * 1 : request sending and receiving
 * 2 : request sending and receiving + show protocol errors */
#define DEBUG 0


/* Variabili globali */
ESP8266WiFiMulti multiWifi;
/* Istanza di InlfuxDbClient */
InfluxDBClient client(INFLUXDB_URL, INFLUXDB_ORG, INFLUXDB_BUCKET, INFLUXDB_TOKEN,
                      InfluxDbCloud2CACert);

/* Measure = tabella in cui inserire i dati */
Point sensors("MyMeasure");

/* creo l'istanza di DHT11 */
DHT dht(DHTPIN, DHTTYPE); 

/* sensore barometrico */
SFE_BMP180 pressure;

/* sensore di particolato */
SPS30 sps30;


/* misure provenienti dai sensori */
double t = 0.0;
double h = 0.0;
double p = 0.0;
double board_temperature = 0.0;
/* wind_speed è al momento uno stub per eventuale anemometro */
double wind_speed = 0.0;

/* Misure del particolato da SPS-30 */
struct sps_values val;


void setup() {
  /* inizializzo la comuincazione seriale */
  Serial.begin(115200);
  
  /* inizializzo il DHT */
  dht.begin();

  /* inizializzo il sensore barometrico */
  initBMP180();

  /* inizializzo SPS30 */
  initSPS30();

  /* inizializzo la connessione WiFi */
  initWiFi();

  /* preparo la connessione su influxDB */
  initInfluxDB();

  /* leggo temperatura e umidità */
  readDHT11();

  /* leggo la pressione atm */
  readBMP180();

  /* leggo il sensore di particolato */
  readSPS30();

  /* scrivo su InfluxDB */
  writeToInfluxDB();
  
  /* vado in stand-by (tempo in microsecondi) */
  ESP.deepSleep(SLEEP_TIME);  
  /* all'arrivo dell'interrupt riparte dal reboot */

} /* end funzione setup */

void loop() {
/*
  Versione con il deep sleep
  WAKE --- RST
  Non entriamo mai qui
*/
}



void initWiFi() {
  WiFi.mode(WIFI_STA);
  multiWifi.addAP(WIFI_SSID, WIFI_PASSWORD);
  Serial.println("Connessione Wi-Fi in corso...");
  while(multiWifi.run() != WL_CONNECTED){
    Serial.print(".");
    delay(100);
  }
  Serial.println("Connessione Wi-Fi stabilita!");
}


void initInfluxDB() {
  /* preparo la connessione su influxDB */
  /* aggiungo i tag nel Point per salvare in modo ordinato i dati sul server */
  sensors.addTag("host", "MyHost");
  sensors.addTag("location", "MyLocation");
  sensors.addTag("environment", "MyEnvironment");

  /* testiamo la connessione  su influxDB */
  if(client.validateConnection()){
    Serial.println("Connessione a influxDB stabilita!");
  }else {
    Serial.println("Errore connessione a InfluxDB!");
    Serial.println(client.getLastErrorMessage());
  }  
}


void initBMP180(){
  /* Inizializza il sensore barometrico. 
     Importante per acquisire i valori 
     di calibrazione memorizzati nel device  */
  if (pressure.begin())
    Serial.println("Inizializzazione BMP180 eseguita!");
  else
  {
    Serial.println("Err. Inizializzazione BMP180!\n\n");
  }
}


void initSPS30() {
  /* Inizializza il sensore SPS30
  */


  /*  Inizializzazione seriale 
      già fatta all'inizio del setup
      Serial.begin(115200); 
  */

  Serial.println("SPS-30: connessione in corso");

  /* imposta debug level */
  sps30.EnableDebugging(DEBUG);

  /* imposta pin per softserial e Serial1 su ESP32 */
  if (TX_PIN != 0 && RX_PIN != 0) sps30.SetSerialPin(RX_PIN,TX_PIN);

  /* inizializza canale di comunicazione */
  if (! sps30.begin(SP30_COMMS))
    Serial.println("SPS-30: err. inizializz. canale di comunicazione");

  /* controlla la connessione */
  if (! sps30.probe()) Serial.println("err. connessione con SPS30.");
  else  Serial.println(F("Connesso con SPS30."));

  // reset SPS30 connection
  if (! sps30.reset()) Serial.println("SPS-30: errore azzeramento");

  /* legge informazioni di sistema (qui non ci interessano) 
     GetDeviceInfo(); */

  /* inizia la misurazione */
  if (sps30.start()) Serial.println("SPS30: avvio misurazione");
  else Serial.println("err. misurazione");
 
  /*  Warning relativo alla connessione I2C. Qui non ci interessa
      perché utilizziamo la seriale
  if (SP30_COMMS == I2C_COMMS) {
    if (sps30.I2C_expect() == 4)
      Serial.println(F("Sono disponibili solo i dati di Massa per limiti del buffer I2C! \n"));
  } */

}


bool readSPS30()
{
  uint8_t ret, error_cnt = 0;

  /* loop per lettura dati */ 
  do {

    ret = sps30.GetValues(&val);

    /* i dati potrebbero non essere pronti */
    if (ret == ERR_DATALENGTH){

        if (error_cnt++ > 3) {
          /* ErrtoMess((char *) "Err. lettura dati: ",ret); */
          return(false);
        }
        delay(1000);
    }

    /* if other error */
    else if(ret != ERR_OK) {
      /* ErrtoMess((char *) "Err. lettura dati: ",ret); */
      return(false);
    }

  } while (ret != ERR_OK);

 return(true);
}



/* funzione che legge dal sensore DHT11 */
void readDHT11(){
  t = dht.readTemperature();
  h = dht.readHumidity();
  Serial.print("temperature: ");
  Serial.print(t);
  Serial.print(" humidity: ");
  Serial.println(h);  
}

/* funzione per scrivere su influxDB */
void writeToInfluxDB(){
  sensors.clearFields();
  sensors.addField("temperature", t);
  sensors.addField("humidity", h);
  sensors.addField("pressure", p);
  sensors.addField("board_temperature", board_temperature);
  sensors.addField("wind_speed", wind_speed);
  
  /* SPS30 */
  sensors.addField("number_of_pm0", val.NumPM0 );
  sensors.addField("mass_of_pm1", val.MassPM1);
  sensors.addField("number_of_pm1", val.NumPM1 );
  sensors.addField("mass_of_pm2", val.MassPM2);
  sensors.addField("number_of_pm2", val.NumPM2);
  sensors.addField("mass_of_pm4", val.MassPM4);
  sensors.addField("number_of_pm4", val.NumPM4);
  sensors.addField("mass_of_pm10", val.MassPM10);
  sensors.addField("number_of_pm10", val.NumPM10);  
  sensors.addField("average_part_size", val.PartSize);  

  Serial.print("Scrittura su InfluxDB: ");
  Serial.println(sensors.toLineProtocol());

  /* ora scriviamo sul server */
  if(!client.writePoint(sensors)){
    Serial.print("Err. scrittura su InfluxDB!");
    Serial.println(client.getLastErrorMessage());
  }  
}


/* legge il sensore barometrico */
void readBMP180() {
  char status;
  double T,P,p0,a;
  
  Serial.println();
  Serial.print("Quota stazione: ");
  Serial.print(ALTITUDE,0);
  Serial.print(" metri, ");
  Serial.print(ALTITUDE*3.28084,0);
  Serial.println(" piedi");

  /* Se necessita misurare la quota e non la pressione, occorre fornire
     la pressione a quota zero.

     Necessita leggere la temp. prima della pressione
 
     Inizio dela lettura della temp.
     Se la richiesta è ok viene restituito il num di ms di attesa
     Altrimenti restituisce 0 
  */
  status = pressure.startTemperature();
  if (status != 0)
  {
    /* Attende il termine della misura */
    delay(status);

    /* Restituisce la lettura del velore di temp.
       La misura è contenuta nella var. T
       La funz restituisce 1 se ok, 0 altrimenti
    */

    status = pressure.getTemperature(T);
    if (status != 0)
    {

      /* var per InfluxDB  */
      board_temperature = T;
      
      Serial.print("temperature: ");
      Serial.print(T,2);
      Serial.print(" deg C, ");
      Serial.print((9.0/5.0)*T+32.0,2);
      Serial.println(" deg F");
     
      /* Avvia la misura di pressione
         Il parametro è un settaggio dell'oversampling, da 0 a 3 (+ alta la risoluzione, maggiore è l'attesa)
         La funz restituisce 1 se ok, 0 altrimenti
      */
      status = pressure.startPressure(3);
      if (status != 0)
      {
        /* Attesa completamento della misura */
        delay(status);


        /* Restituisce la lettura del velore di temp.
           La misura è contenuta nella var. P
           La funz restituisce 1 se ok, 0 altrimenti
        */
        status = pressure.getPressure(P,T);
        if (status != 0)
        {
          /* p è la var per InfluxDB */
          p = P;
          Serial.print("absolute pressure: ");
          Serial.print(P,2);
          Serial.print(" mb, ");
          Serial.print(P*0.0295333727,2);
          Serial.println(" inHg");

          /* Il sensore di pressione ritorna la press. assoluta che varia con l'altezza. 
             Per rimuovere l'effetto dell'altezza utilizzare la funzione di livello del mare e
             la quota corrente. 
             Parametri: P = pressione assoluta in mb, ALTITUDE = quota corrente in m
             Risultato: p0 = pressione in mb normalizzata al livello del mare
          */

          p0 = pressure.sealevel(P,ALTITUDE); // we're at 1655 meters (Boulder, CO)
          Serial.print("pressione relativa: ");
          Serial.print(p0,2);
          Serial.print(" mb, ");
          Serial.print(p0*0.0295333727,2);
          Serial.println(" inHg");


          /* Viceversa, se necessita determinare la quota dalla lettura di pressione,
             utilizzare la funzione di altezza con la pressione a quota zero.
             parametri: P = press. assoluta in mb, p0 = press a quota zero in mb
             Risultato: a = altezza in m.
          */

          a = pressure.altitude(P,p0);
          Serial.print("quota calcolata: ");
          Serial.print(a,0);
          Serial.print(" metri, ");
          Serial.print(a*3.28084,0);
          Serial.println(" piedi");
        }
        else Serial.println("err lettura pressione\n");
      }
      else Serial.println("err avvio misura pressione\n");
    }
    else Serial.println("err lettura temperatura\n");
  }
  else Serial.println("err avvio misura temperatura\n");

}
