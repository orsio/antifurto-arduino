/*  Questa e' la  versione dell'antifurto su touch screen e lettore impronte digitali
 lo schema si basa sui seguenti parametri e assunzioni
 

 *******************************************************************
 CARATTERISTICHE MODULO LCD TOUCH
 Power consumption requierments  4.5/5.5V 250mA max
 Resolution 320x240     View angle 60~120
 SPI pin-saving communication method, pins 10,11,12,13.
 Backlight controllable via programming
 TFT Screen control pins 4,5,6,7.
 Touch function pins A0, A1, A2, A3. 
 Dimension: 72.5x54.7x18,
 2.8 inch screen
 LCD color 65k
 
 Quindi sembrano restare liberi: gli ingressi analogici A4 A5 
 E GLI I/O DIGITALI in pratica  2 3    8 9 
 
 
 *********************  ALTRI INPUT ANALOGICI UTILIZZATI **************************
 
 A4  = Sensore porta                  tipo input   5V = porta chiusa   0 V= porta aperta
 A4  = Sensore lettore impronte       tipo input   5V = no dito        3 V= richiesta lettura
 A5  = Sensore Microonde              tipo input   5V = nessun allarme 0 V= allarme
 
 ***********************************************************************************
 *    schema connettore esterno  lato arduino  lato esterno
 *
 *     Vin                          1             8
 *     5 V                          2             9
 *     analog in 5                  3            10      
 *     Digital I/0  3               4            11            
 *     Digital I/O  2 partizionato  5            12
 *     Digital io   8               6             2               
 *     Digital I/0  9               7             3               
 *     Analog in 4                  9             5
 *     GND                         10             6                
 ***********************************************************************************
 *   accorgimento per scrivere le costanto in memoria flash usando avr/pgmspace.h
 *   const char message[] PROGMEM = "Some very long string...";
 *   char buffer[30];
 *   strcpy_P(buffer, message);
 ************************************************************************************
*/

const char versione[] = " INI ver. 13.0";

// #include <stdint.h>                        //   per lo stantard touch screen
// #include "SoftwareSerial.h"                //   per gestire la comunicazione seriale
#include "FPS_GT511C3.h"                      //   per il display  TFT
#include <TouchScreen.h>                      //   per il touch screen standard
#include <TFTv2.h>                            //   per il touch screen 
#include <SPI.h>                              //   per il real time clock
#include <avr/wdt.h>                          //   watchdog
#include <avr/pgmspace.h>                     //   per scrivere le costanti nella memoria flash invece che in ram                

#define RED           0xf800
#define GREEN	        0x07e0
#define BLUE	        0x001f
#define BLACK	        0x0000
#define YELLOW	      0xffe0
#define WHITE        	0xffff
#define CYAN	      	0x07ff	
#define PINK		      0xf810	
#define GRAY1         0x8410  
#define GRAY2         0x4208
#define DARK_GREEN  	0x02e0
#define MAGENTA	      0xf8df
#define fpsrx  3 
#define fpstx  2 


//  *********************  ALTRI OUTPUT UTILIZZATI ************************************
//  9 = Buzzer
//  8 = Sirena                tipo output  high = sirena attivata
//  3 = FPS fingerprint       digital pin 3 (arduino rx, fps tx)
//  2 = FPS fingerprint       digital pin 2(arduino tx - 560ohm resistor fps tx - 1000ohm resistor - ground)
//                            this brings the 5v tx line down to about 3.2v so we dont fry our fps

#define BUZZER    9
#define SIRENA    8 
#define PORTONE   5        // analog input 5
#define MICROONDE 4        // analog input 4
#define nota 3000          // nota del buzzer

// ************************************ STATI DEL SISTEMA

boolean RITARDO              = false;             //  FLAG DEL RITARDO IN PARTENZA 
                                                  //  si accende automaticamente se inserisco l'allarme dall'interno 
                                                  //  e poi si reimposta ad off automaticamente                                                  
boolean ATTENDI_USCITA       = false;             //  se vero devo aspettare uscita
boolean ATTENDI_ENTRATA      = false;             //  se vero devo aspettare entrata
boolean ALLARME_INSERITO     = false;             //  se vero ALLARME INSERITO
boolean STATO_SIRENA         = false;             //  se vero SIRENA ACCESA 
boolean ABILITA_SIRENA       = true;              //  se vero SIRENA ABILITATA
boolean ABILITA_INTERNI      = true;              //  MICROONDE e FINESTRE ABILITAT0
boolean STATO_PORTA          = false;             //  se vero porta ingresso aperta
boolean STATO_FINESTRE       = false;             //  se vero finestre aperte
boolean STORIA_ALLARME       = false;             //  storico allarme per segnalare  al display che c'è stato un allarme
boolean yesnot               = false;             //  return della risposta si o no: true significa si 
byte    stato_tastiera       = 0;                 // definisco lo stato della tastiera   
byte    conta_allarmi        = 0;                 // contatore degli allarmi se maggiore di 5 smetto di suonare

int x          = 0;                 // variable spare
int y          = 0;                 // variable spare
int z          = 0;                 // variable spare
int leggi      = 0;                 // funzione di menu scelta
int funzione   = 0;                 // funzione di menu scelta
long t_valore;                      // E' il valore del numero digitato in tastiera se in valore e' -1 
                                    // significa  che la la routine tastiera e' terminata in modo anomalo 
                                  
// ******************************************************************      di seguito l'elenco dei pin validi 
long pin01 = 369;                   //    pin 01         PARTO CON 369 PER COMODITA' POI VA CAMBIATO
long pin02 = 0;                     //    pin 02
long pin03 = 0;                     //    pin 03
long pin04 = 91087;                 //    pin di default nascosto DATA NASCITA LAURA
int  pin   = 0;                     //    numero del pin in uso ( da 1 a 3 )    
       
// *******************************************************************      definisco le variabili per la data 
unsigned long secondi   =  1;       // correnti     
unsigned long minuti    =  1;       // correnti   
unsigned long ore       =  1;       // correnti 
unsigned long giorni    =  0;       // conta  giorni dall'accensione  
unsigned long ttimer    =  0;       // usato per la lettura dei secondi
unsigned long ttimer1   =  0;       // usato per la lettura dei secondi

// *******************************************************************      variabili per il touch

#define quantapressione 200         // quanta pressione per premuto
#define troppapressione 3000        // per i falsi positivi di pressione

// ********************************************************************      variabili per timeout 
long tempo;                         // usati per il timeout
long tempo1;                        // usati per il timeout del display
long tempo_IO;                      // usati per il timeout  dell'uscita delle persone
long tempo_SIR;                     // usati per il timeout  della sirena

#define temposcaduto     40         // timeout DI DISPLAY
#define temposcaduto_IO  60         // timeout di ENTRATA E USCITA
#define temposcaduto_SIR 90         // timeout di SIRENA


// *****************  ARRAY DI STRINGHE *******************************

char       appoggia[20];         
const char premiperuscire[] PROGMEM  = {"Tocca per uscire"};
const char Cgestione[]      PROGMEM  = {"GESTIONE"};
const char Ccancella[]      PROGMEM  = {"CANCELLA"};
const char Cimpronte[]      PROGMEM  = {"IMPRONTE"};
const char Cpin[]           PROGMEM  = {"PIN"};
const char Cregola[]        PROGMEM  = {"REGOLA"};
const char Celenco[]        PROGMEM  = {"ELENCO"};
const char Cora[]           PROGMEM  = {"ORA"};  
const char Cconsulta[]      PROGMEM  = {"CONSULTA"};  
const char Cstorico[]       PROGMEM  = {"STORICO"};
const char Csirena[]        PROGMEM  = {"SIRENA"};  
const char Conoff[]         PROGMEM  = {"ON/OFF"}; 
const char Cmicroonde[]     PROGMEM  = {"MICROONDE"};
const char Cesci[]          PROGMEM  = {"E X I T"};   
const char Callarme[]       PROGMEM  = {"ALLARME"};
const char Cfinestre[]      PROGMEM  = {"FINESTRE"};
const char Cqualcuno[]      PROGMEM  = {"TASTINO INPR."};
const char Cporta[]         PROGMEM  = {"PORTA"};
const char Cchiusa[]        PROGMEM  = {"CHIUSA"};
const char Caperta[]        PROGMEM  = {"APERTA"};
const char Cinterni[]       PROGMEM  = {"INTERNI"};
const char Csconosciuto[]   PROGMEM  = {"SCONOSCIUTO"};
const char Critardo[]       PROGMEM  = {"RITARDO"};
const char Cinserisci[]     PROGMEM  = {"INSERISCI"};
const char Con[]            = "ON";
const char Coff[]           = "OFF";
 

//   ********************************************************************      
//   *********   il file storico registra gli ultimi 10 accessi  

int Schi[]    = {  0, 0, 0, 0, 0, 0, 0, 0, 0, 0};      // chi è stato impronta (1-20)   91 = porta 92 = microonde 93 = finestre
int Sore[]    = { -1,-1,-1,-1,-1,-1,-1,-1,-1,-1};      // metto inizialmente le ore a -1 per non mostrare le righe vuote
int Sminuti[] = {  0, 1, 2, 3, 4, 5, 6, 7, 8, 9};
int Sgiorni[] = {  0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
int Sstato[]  = {  0, 0, 0, 0, 0, 0, 0, 0, 0, 0};

//   ******************  INIZIALIZZAZIONI ********************************

TouchScreen ts = TouchScreen(XP, YP, XM, YM);  //  init TouchScreen port pins
FPS_GT511C3 fps(3,2);                          //  il  3 arduino riceve il 2 arduino trasmette

//   ******************************************************************************
//   *             SETUP
//   ******************************************************************************
void setup()                       
{
           // inizializzazione del sistema
           // azzero lo stato_tastiera
           // avvio il timer di tastiera

  delay (100);
  pinMode      (PORTONE,  INPUT); 
  pinMode      (MICROONDE,INPUT);
  pinMode      (SIRENA,   OUTPUT); 
  digitalWrite (SIRENA,   LOW); STATO_SIRENA=false;

  Serial.begin(9600);

  Tft.TFTinit();              // init TFT library
  Tft.fillScreen();           // azzero lo schermo;

  fps.Close();
  fps.Open();
  fps.SetLED(false);

  TFT_BL_OFF;
  TFT_BL_ON;

  Tft.drawString(versione,5,10,2,WHITE);   
  suona (5);   

  delay(100);        
  digitalWrite(SIRENA, LOW);     
  STATO_SIRENA        = false;
  ALLARME_INSERITO    = false; 

                                               
  wdt_disable();                                       // disabilito il  watchdog
  wdt_enable(WDTO_8S);                                 // ri-abilito il watchdog a 8 secondi

  stato_tastiera=0;                                   // lo stato_tastiera a 0 e' lo stato_tastiera iniziale  
  ttimer1  =  millis() / 1000;
  ttimer   =  millis() / 1000;    
  Tft.fillScreen();     
  
  impostaora  ();                                     // si imposta l' ora
}

//    *******************************************************************************
//                                    LOOP PRINCIPALE
//       Controllo degli eventi che possono essere:  il sensore microonde , 
//       i contatti alle finestre, il contatto sulla porta, la pressione di un tasto sulla tastiera  
//       oppure il timer  in owerflow  
//    *******************************************************************************

void loop()
{
    wdt_reset();                                                                 //resettoil watchdog  

//   **********************************************************************
//       menu loop: CONTROLLO se la porta e aperta o premuto tastino del dito 
//
//       L'analog read PORTONE  può assumere i seguenti valori
//                  < 200  premuto il tastino impronte
//                  > 800  porta ingresso aperta
//                  compreso tra 199 e 800  la porta è chiusa 
//
//   **********************************************************************

    leggi=0; 
    leggi= analogRead (PORTONE);                 //      
    if (leggi < 200)  leggi_impronta();          //   e' stato premuto il tastino delle impronte  

 //    *************************************************************************************************
 //       menu loop controllo se siamo con sirena accesa o nello stato di attesa dell'uscita / entrata
 //       in questo stato non devo controllare i sensori ma solo il lettore di impronte e la tastiera
 //    **************************************************************************************************   

    if (STATO_SIRENA)  goto salta_i_controlli;  //   se la sirena è accesa salto i successivi controlli 

     if (ATTENDI_USCITA)                        //   conto alla rovescia PER USCIRE
           {
            conto_alla_rovescia ();           
            goto salta_i_controlli;  
           }  
           
     if (ATTENDI_ENTRATA)                      //   conto alla rovescia PER ENTRARE ( è già scattato un allarme )
          {
           conto_alla_rovescia ();            
           goto salta_i_controlli;   
          }     
     
 //   ***********************  qui proseguono i controlli *********************************************
    
    if (leggi > 800)  {                                                      //  la porta è  aperta                
                       if (ALLARME_INSERITO)  
                          { 
                          Schi[0]  = 991;                                    // scrivo che chi è la porta
                          scattaallarme ();                                 //  se l'allarme è inserito vai a scattaallarme 
                          STATO_PORTA=true;    
                          }
                       if (!STATO_PORTA)
                          { 
                            STATO_PORTA=true; 
                            menu_iniziale(); 
                          } 
                      }
    if (leggi >199 && leggi <801 && STATO_PORTA==true )                      // PORTA CHIUSA
            { 
            STATO_PORTA=false ; 
            menu_iniziale();  
            }   

  //    *******************************************************************                                                                                                    
  //     menu loop : CONTROLLO se scattato microonde
  //    *********************************************************************   
  //             L'analog read MICROONDE controlla il microonde e le finestre
  //                                  NOTA per ora è abilitato solo il microonde
  //            puo' assumere i seguenti valori 
  //                                             < 50 microond + finestre
  //                                             < 400 finestra 
  //                                             < 570 microonde
  //                                             > 569 tutto chiuso
  //    **********************************************************************
    
     leggi= analogRead (MICROONDE);
     
     if ( ABILITA_INTERNI)
             { 

                   if   (leggi > 569)   STATO_FINESTRE = false;     // tuttochiuso
                   
                   if   (leggi < 570 && ALLARME_INSERITO )          // solo microonde 
                         {                                                                      
                           Schi[0]  = 992;                           // scrivo che chi è microonde
                           scattaallarme ();    
                         }
                           
                   if  (leggi < 400 && ALLARME_INSERITO)            // solo finestre
                         { 
                           Schi[0]  = 993;                           // scrivo che chi è la finestra                                                            
                           scattaallarme ();
                           STATO_FINESTRE = true;
                         }                              
                   if  (leggi <  50 && ALLARME_INSERITO)            // microonde + finestre
                         {                                                                      
                          Schi[0]  = 993;                           // scrivo che chi è la finestra 
                          scattaallarme ();  
                          STATO_FINESTRE = true;
                         }
              }

 //       ******************************************************************
 //       menu loop controllo il timer per eventualmente azzerare lo schermo 
 //       salto anche questo controllo se la sirena è accesa
 //       ****************************************************************** 
  
       tempo =millis()/1000; 
         if (tempo-tempo1 > temposcaduto )             // SE SI SCADUTO IL TEMPO 
          {        
            Tft.fillScreen(); 
            TFT_BL_OFF;                                                 
            tempo1=tempo;
            stato_tastiera = 0;                  
         }    

  salta_i_controlli:      //  ************************    LABEL SALTA I CONTROLLI

  //       *****************************************************************
  //       menu loop controllo il timer per eventualmente spegnere la sirena
  //       se la sirena è accesa ma non abilitata ( quindi non suona ) suono un beep
  //       ******************************************************************           
  
  if(STATO_SIRENA)                                                                 // se on significa che la sirena è accesa
     {    
       if (!ABILITA_SIRENA) {  tone (BUZZER, nota); delay(30); noTone  (BUZZER); delay(30);  } // se sirena disabilitata simulo  un allarme a cicalino
          
       tempo = millis()/1000;  
       if (tempo -tempo_SIR > temposcaduto_SIR )                  // SE SCADUTO TEMPO SIRENA LA SPENGO
             {  
             digitalWrite(SIRENA, LOW);   
             STATO_SIRENA=false;   
             }  
     }      

  //      ******************************************************************
  //      menu  loop: Controllo se è stato_tastiera premuta la tastiera TFT 
  //      e nel caso leggo la tastiera del TFT 
  //      ******************************************************************

   Point p = ts.getPoint();                                     // map the ADC value read to into pixel co-ordinates

  if ((p.z > quantapressione) and (p.z < troppapressione))      // e stata premuta la tastiera 
     {     
      p.x = map(p.x, TS_MINX, TS_MAXX, 0, 240);   
      p.y = map(p.y, TS_MINY, TS_MAXY, 0, 320);

     suona (1);                                                  // ora devo analizzare cosa devo fare o meglio cosa stavo facendo  
     TFT_BL_ON;

    tempo =millis() /1000;  tempo1=tempo;

    yesnot=false;  
    if ((p.y > 245) and (p.x <= 120)) yesnot= true;              // setto yesnot a true se premuto si                                                     
 
    switch (stato_tastiera) 
    {                                                             
    case 0:       menu_iniziale ();   break ;                                           // menu principale   
    case 1:   
      if (yesnot==true)  { menu_pin ();  break ; }                                      // tasto si no setup del menu principale
      if (yesnot==false) { stato_tastiera=0;  TFT_BL_OFF   Tft.fillScreen();  break ; } // azzero lo schermo perchè premuto no in setup         
    case 2:  break ;                                                                    // non usato
    case 3:  menu_accetta_funzione (p.x,p.y);  break ;                                  // menu funzioni accetta tasto   
    case 4:                                                                             //  cancellazione impronte              
      if (yesnot==true)  { cancella_impronte_esegui   () ;  break ; }                   // si o no nella cancellazione impronte  
      if (yesnot==false) { stato_tastiera=0 ;  menu_iniziale () ; break ;  }                     
    case 5:   
      if (yesnot==true)  {  nuova_impronta_esegui ();   break ;  }                      // menu gestione impronte - cancella impronta
      if (yesnot==false) {  cancellaimpronte();    break ; }
    case 6:                                                                             // si o no per modifica pin 
      if (yesnot==true)  {  modificapin ();   break ;  }                                //  modifica il pin 
      if (yesnot==false) {  stato_tastiera=0 ;  menu_iniziale () ; break ;  }
    default:  {suona (20);  stato_tastiera =0 ; Tft.fillScreen(); }                     // if nothing else matches, do the default
    }                       
  }  
}              

//    **********************   fine menu loop ******************************************************
//    ***********************************************************************************************

//   **********************************************************************************************************
//    menu principale    0100 
//   questo è il menu principale che viene attivato con stato_tastiera a zero se si preme il touch screen
//   ***********************************************************************************************************
void menu_iniziale ()           // si entra con stato_tastiera = 0       
{           
  Tft.fillScreen(); 
  leggiora();  tempo =millis() /1000;   tempo1=tempo ;              // azzero lo schermo e avvio il timer     
  TFT_BL_ON;                                                        // controllo per tutto ok 
  if (STORIA_ALLARME)  { Tft.drawString ("SCATTATO",30,6,3,RED); strcpy_P(appoggia, Callarme); Tft.drawString (appoggia,35,38,3,RED); }
     else   {            Tft.drawString ("CIAO LAURA",21,6,3,WHITE);   Tft.drawString ("TUTTO OK",34,38,3,GREEN); }

  if (ore >9)       {   Tft.drawNumber (ore,40,68,4,YELLOW);      } 
  if (ore <10)      {   Tft.drawNumber (ore,70,68,4,YELLOW);      }     
  if (minuti > 9)   {   Tft.drawNumber (minuti,115,68,4,YELLOW);  }
  if (minuti < 10)  {   Tft.drawString ("0",115,68,4,YELLOW);  Tft.drawNumber (minuti,140,68,4,YELLOW);  }
  Tft.drawString    (":",97,74,3, YELLOW); 

//      ************************ parametri posizionali del display  135 155 175 195 215
#define P_ritardo     135
#define P_porta       155
#define P_sirena      175
#define P_microonde   195
#define P_finestra    215

  strcpy_P(appoggia, Callarme);  
       if (ALLARME_INSERITO)     
                          {        Tft.drawString (appoggia,15,110,3, RED);   Tft.drawString (Con,  150,110,3,RED);  }
                          else  {  Tft.drawString (appoggia,15,110,3, GREEN); Tft.drawString (Coff, 150,110,3,GREEN);}
                    
  strcpy_P(appoggia, Csirena);    Tft.drawString (appoggia,4,P_sirena,2,GREEN);
       if (ABILITA_SIRENA)        Tft.drawString (Con,   140,P_sirena,2,GREEN);       
                          else    Tft.drawString ("---", 140,P_sirena,2,RED)  ; 

  strcpy_P(appoggia, Cporta);     Tft.drawString   (appoggia,4,P_porta,2,    GREEN);
          if (STATO_PORTA)   { strcpy_P(appoggia, Caperta);  Tft.drawString   (appoggia,140,P_porta,2,RED);    }
                     else    { strcpy_P(appoggia, Cchiusa);  Tft.drawString   (appoggia,140,P_porta,2,GREEN);  }  
  
  strcpy_P(appoggia, Cmicroonde); Tft.drawString   (appoggia,4,P_microonde,2,GREEN); 
  strcpy_P(appoggia, Cfinestre);  Tft.drawString   (appoggia,4,P_finestra,2, GREEN);
         if (ABILITA_INTERNI)    
                     { 
                     Tft.drawString (Con,  140,P_microonde,2,GREEN);  
                       if (STATO_FINESTRE)        
                          { 
                          strcpy_P(appoggia, Caperta);  
                          Tft.drawString   (appoggia,140,P_finestra,2,RED);       
                          }   
                          else    
                          { strcpy_P(appoggia, Cchiusa);  
                          Tft.drawString   (appoggia,140,P_finestra,2,GREEN);     
                          }   
                     }                   
            else   
                     { 
                     Tft.drawString ("---", 140,P_microonde,2,RED);    
                     Tft.drawString ("---", 140,P_finestra,2,RED);  
                     }
  
  //    ************************************************************
  
  Tft.drawString ("SETUP ?",70,238,2,WHITE); 

  siono ();
  stato_tastiera=1;                 // lo stato ad 1 per accettare si o no 

}

//   *******************************************************
//    menu_pin   menu password accetta il pin 
//   *******************************************************

void menu_pin  ()                                              
{ 
  wdt_reset();                                              // resetto il watchdog  
  tempo =millis()/1000;   tempo1=tempo;                     // resetto il timer di tastiera
  pin=0;
 
  tasti (50,5);                                             // la funzione 50 accetta il pin
   
  if ( t_valore == pin01) {  pin = 1;   }
  if ( t_valore == pin02) {  pin = 2;   }
  if ( t_valore == pin03) {  pin = 3;   }
  if ( t_valore == pin04) {  pin = 4;   }                  // pin di default
  if ( t_valore == 0)     {  pin = 0;   }                  // se digita zero non vale 


  if ( pin > 0)         //                            **************   
      {
       Tft.drawString ("PIN  OK",10,40,2, GREEN); 

      // *************************   E' STATO DIGITATO UN PIN GIUSTO **************************
      //
      //                          azzero tutti gli stati di allarme 
      // **************************************************************************************
     if (STATO_SIRENA || ALLARME_INSERITO)         // significa stato sirena OR allarme inserito
                    {                              // se la sirena è accesa spegnila                                                                                            
                    Sstato[0] =0;  
                    Schi[0] = (pin + 60);          //  da 60 a 64 sono i valori per pin da tastiera
                    scrivilog();                   // registro che ho spento l'allarme da tastiera  Schi a meno 1 significa da tastiera
                    } 
    
      STATO_SIRENA=false;       
      digitalWrite(SIRENA, LOW);
      ALLARME_INSERITO = false ;
      RITARDO          = false ;   
      ATTENDI_USCITA   = false ; 
      ATTENDI_ENTRATA  = false ;
      STORIA_ALLARME   = false ;
      conta_allarmi    = 0;                 // azzero anche il contatore degli allarmi
      tempo_IO         = 0;
             
                   
// **********************************************************************************
//              ORA SI DISEGNA L'ELENCO DELLE FUNZIONI
// **********************************************************************************
    Tft.fillScreen();    

    Tft.drawString ("ELENCO FUNZIONI",20,5,2, YELLOW);

    Tft.fillRectangle ( 1, 25,120,60,    GRAY1);     //  X   Y  LUNGHEZZA AMPIEZZA
    Tft.fillRectangle ( 121, 25,120,60,  GRAY2);
    Tft.fillRectangle ( 1, 85,120,60,    GRAY2);  
    Tft.fillRectangle ( 121, 85,120,60,  GRAY1); 
    Tft.fillRectangle ( 1,145,120,60,    GRAY1);  
    Tft.fillRectangle ( 121,145,120,60,  GRAY2);
    Tft.fillRectangle ( 1,205,240,60,      RED);  
  //  Tft.fillRectangle ( 121,205,120,60,  RED); 
    Tft.fillRectangle ( 1,265,240,60,     BLUE);  
   
    strcpy_P(appoggia, Cgestione);  Tft.drawString (appoggia,4,35,2,     WHITE);  
    strcpy_P(appoggia, Cimpronte);  Tft.drawString (appoggia,4,60,2,     WHITE);            // codice 1 gestione impronte
    strcpy_P(appoggia, Cgestione);  Tft.drawString (appoggia,123,35,2,   WHITE);  
    strcpy_P(appoggia, Cpin);       Tft.drawString (appoggia,123,60,2,   WHITE);            // codice 2 gestione pin
    strcpy_P(appoggia, Cregola);    Tft.drawString (appoggia,4,95,2,     WHITE);  
    strcpy_P(appoggia, Cora);       Tft.drawString (appoggia,4,120,2,    WHITE);            // codice 3 regola ora
    strcpy_P(appoggia, Cconsulta);  Tft.drawString (appoggia,123,95,2,   WHITE);  
    strcpy_P(appoggia, Cstorico);   Tft.drawString (appoggia,123,120,2,  WHITE);            // codice 4  consulta storico
    strcpy_P(appoggia, Csirena);    Tft.drawString (appoggia,4,155,2,   YELLOW);  
    strcpy_P(appoggia, Conoff);     Tft.drawString (appoggia,4,180,2,   YELLOW);            // codice 5  sirena on off
    strcpy_P(appoggia, Cinterni);   Tft.drawString (appoggia,123,155,2, YELLOW);  
    strcpy_P(appoggia, Conoff);     Tft.drawString (appoggia,123,180,2, YELLOW);            // codice 6  interni on off 
    strcpy_P(appoggia, Cinserisci); Tft.drawString (appoggia,10,230,2,  YELLOW);            // codice 7  inserisci allarme 
    strcpy_P(appoggia, Callarme);   Tft.drawString (appoggia,130,230,2, YELLOW);            // codice 8  inserisci allarme     
    strcpy_P(appoggia, Cesci);      Tft.drawString (appoggia,50,285,3,   GREEN);            // codice 9  EXIT

    tempo =millis()/1000;   tempo1=tempo;
    stato_tastiera=3;                                     // lo stato_tastiera a 3 e' per la lettura della scelta del menu funzioni                                                                                                                 

    }   // ************  fine dell' if per pin giusto *********************     
    else                          
       {
         Tft.drawString (" PIN  ERRATO",1,120,3, RED);                  
         suona (3);  
         delay (10); 
         stato_tastiera=0;                                 
         premiperexit ();                      // torno al menu iniziale   
       }
}

// *******************************************************************************
// menu delle funzioni  -- analisi del tasto 
// *******************************************************************************        
void menu_accetta_funzione (int xx, int yy)            
{ 
  funzione =0;  

  if ((yy >= 20  ) and (yy <= 84))  {  funzione=1;   }    
  if ((yy >= 85  ) and (yy <= 144)) {  funzione=3;   }    
  if ((yy >= 145 ) and (yy <= 204)) {  funzione=5;   }   
  if ((yy >= 205 ) and (yy <= 264)) {  funzione=7;   }   
  if ( xx >= 120 )                  { funzione= funzione+1;   }  
  if ((yy >= 265 ) and (yy <= 320)) {  funzione=10;           }  //  premuto exit        

  switch (funzione) 
  {   
  case 0:   suona (3);           break ;                // sopra per nessuna scelta
  case 1:   gestione_impronte(); break ;                // gestione impronte
  case 2:   elencodeipin();      break ;                // gestione pin
  case 3:   impostaora();        break ;                // imposta ora 
  case 4:   consultastorico();   break ;                // consulta storico
  case 5:   escludisirena ();    break ;                // escludi / includi sirena 
  case 6:   escludiinterni ();   break ;                // escludi interni escludi / includi microonde e finestre 
  case 7:   inserisci_allarme_da_tastiera (); break ;   //  abilitazione dell'allarme da tastiera
  case 8:   inserisci_allarme_da_tastiera (); break ;   //  abilitazione dell'allarme da tastiera
  case 9:   suona (4);           break ;                // fuori  limite
  case 10:  suona (2);   
            delay (100); 
            stato_tastiera=0; 
            Tft.fillScreen();  
            TFT_BL_OFF;    
            break ;                // exit

  default:    
            suona (4);  
            stato_tastiera=0;                      // if nothing else matches, do the default    
  }  

}

//    **********************************************************************
//    ROUTINE TASTI             DISEGNA IL TASTIERINO NUMERICO        
//    valori passati 
//    variabile char t_msg che viene evidenziata nella prima riga PER IL DEBUG 
//    variabile int t_ndigit per il numero di digit da mostrare
//    variabile int t_funzione per il tipo funzione 
//    tipo funzione 10 = digita ora
//    tipofunzione  11 = digita minuti
//    tipofunzione  12 = digita secondi
//    tipo funzione 50 = digita PIN
//    
//    ************************************************************************

void tasti (int t_funzione, int t_ndigit )    
{
  wdt_reset();                                             //resetto il watchdog  
                                      
  char codice[10] =" " ;                      // caratteri del codice
  int ii =0;                                  // stato_tastiera del digit 
  int codicetasto = 0;                        // II è IL FALG per controllare cio che si digita
  int x;                                      // usato nei for next

  t_valore = -1;                              // metto il valore a meno 1 che e' lo stato_tastiera di errore 

  Tft.fillScreen();                           // azzero lo schermo
                                              //  scrivo il messaggio sulla prima riga dell schermo   
  switch (t_funzione) 
  {                                                             
  case 10:  Tft.drawString("Ora ? ",            10,4,3,   WHITE)    ;  break;              // sulla prima riga il messaggio   
  case 11:  Tft.drawString("Minuti ?",          10,4,3,   WHITE)    ;  break;              // sulla prima riga il messaggio  
  case 12:  Tft.drawString("Secondi ?",         10,4,3,   WHITE)    ;  break;              // sulla prima riga il messaggio  
  case 20:  Tft.drawString("persona (1-5) ?",    1,4,2,   WHITE)    ;  break;              // sulla prima riga il messaggio 
  case 50:  Tft.drawString("PIN ?",             10,4,3,   WHITE)    ;  break;              // sulla prima riga il messaggio 
  case 54:  Tft.drawString("N.pin (1-3)?",      10,4,2,   WHITE)    ;  break;              // sulla prima riga il messaggio   
  case 55:  Tft.drawString("valore pin ?",      10,4,2,   WHITE)    ;  break;              // sulla prima riga il messaggio 
  case 99:  Tft.drawString("",                  10,4,2,   WHITE)    ;  break;              // sulla prima riga il messaggio  
  default:  Tft.drawString("??",                10,4,3,   RED)      ;                      // if nothing else matches, do the default
  }

 //   Tft.drawNumber(t_funzione, 210, 4, 2, GREEN);      //  scrivo la funzione  sulla prima righa dell schermo    ( per test)

 // ****************************  ora disegno la tastiera 
  
 // parametri grafici per disegno tastiera
  int l  = 65;    
  int w  = 50;    
  int c1 = 10;   
  int c2 = 85;   
  int c3 = 160;  
  int r1 = 90;   
  int r2 = 150;   
  int r3 = 210;  
  int r4 = 270;  
  int incry =16;  
  int incrx =21; 
    
  Tft.fillRectangle( c1, r1, l,w,GREEN); 
  Tft.fillRectangle( c2, r1, l,w,GREEN); 
  Tft.fillRectangle( c3, r1, l,w,GREEN); 
  Tft.fillRectangle( c1, r2, l,w,GREEN); 
  Tft.fillRectangle( c2, r2, l,w,GREEN); 
  Tft.fillRectangle( c3, r2, l,w,GREEN); 
  Tft.fillRectangle( c1, r3, l,w,GREEN); 
  Tft.fillRectangle( c2, r3, l,w,GREEN); 
  Tft.fillRectangle( c3, r3, l,w,GREEN); 

  Tft.drawNumber(1, c1+incrx, r1+incry, 3, BLACK); 
  Tft.drawNumber(2, c2+incrx, r1+incry, 3, BLACK); 
  Tft.drawNumber(3, c3+incrx, r1+incry, 3, BLACK); 
  Tft.drawNumber(4, c1+incrx, r2+incry, 3, BLACK);
  Tft.drawNumber(5, c2+incrx, r2+incry, 3, BLACK); 
  Tft.drawNumber(6, c3+incrx, r2+incry, 3, BLACK); 
  Tft.drawNumber(7, c1+incrx, r3+incry, 3, BLACK); 
  Tft.drawNumber(8, c2+incrx, r3+incry, 3, BLACK); 
  Tft.drawNumber(9, c3+incrx, r3+incry, 3, BLACK);          

  Tft.fillRectangle( c1, r4, l,w,RED)  ; 
  Tft.drawString("C",c1+incrx,r4+incry,3,  WHITE);     
  Tft.fillRectangle( c2, r4, l,w,GREEN); 
  Tft.drawNumber(0, c2+incrx, r4+incry,3,  BLACK); 
  Tft.fillRectangle( c3, r4, l,w,BLUE) ; 
  Tft.drawString("OK",c3+10,r4+incry,3,    WHITE);  

  Tft.drawRectangle(10, 35, 215,50, BLUE);
  Tft.fillRectangle(12, 37, 210,45,WHITE);
                                                                   
  for (x=0; x<t_ndigit; x++ ) {   codice [x] = '-';   }      //  riempio di trattini quanti indicati da ndigit
  Tft.drawString(codice, 15,50,3 , BLUE); 
  
  wdt_reset();                                               // resetto il watchdog  
  tempo =millis()/1000;  tempo1=tempo;    
                                                               
  do             // *************************** ora si legge la tastiera ********************
  {            
                  
    delay (10);   //  timeout di tastiera

 // #################################  ripeto le seguenti istruzioni del menu loop per controllare il sensore impronte
 // #################################   e la durata della sirena
      leggi= analogRead (PORTONE);                                         // controllo se premuto tastino
      if (leggi < 200) {  leggi_impronta();  ii=6; break; }                //  se si leggi impronta e annulla tutto 
   
       if(STATO_SIRENA)  
               {    
                tempo =millis()/1000;  
                  if (tempo-tempo_SIR > temposcaduto_SIR ) 
                     {  
                      digitalWrite(SIRENA, LOW);   
                      STATO_SIRENA=false;                     
                     }  
                }   
       if (ATTENDI_USCITA)   conto_alla_rovescia ();
       if (ATTENDI_ENTRATA)  conto_alla_rovescia ();
         
  // ################################  fine funzioni ripetute 
   
    tempo =millis()/1000;          
    if (tempo-tempo1 >= temposcaduto )   {  ii=6;   tempo1=tempo;  }    // ii = a sei significa timeout
                                                            
    wdt_reset();      

//  *******************************************************  leggo una cifra  da tastiera
    Point p = ts.getPoint();                                 // a point object holds x y and z coordinates.                       

    if ((p.z > quantapressione) and (p.z < troppapressione))                                       
    {                  
      ii=0;                                                  //  metto il flag ii  a zero
      p.x = map(p.x, TS_MINX, TS_MAXX, 0, 240);              //  map the ADC value read to into pixel co-ordinate     
      p.y = map(p.y, TS_MINY, TS_MAXY, 0, 320); 
      suona (1);
      Tft.fillRectangle(12, 37, 210,42,WHITE);               // azzero il display     

      //   trascodifico p.x e p.y in numeri  
      //   int l =65;    int w = 50;    int c1 = 10;   int c2 = 85;   int c3 = 160;  
      //   int r1= 90;   int r2= 150;   int r3 = 210;  int r4 = 270;  int incry =16;       

      if ((p.x >= 1 ) and (p.x < c2)) {   codicetasto=1;  }
      if ((p.x >= c2) and (p.x < c3)) {   codicetasto=2;  }
      if ( p.x >= c3)                 {   codicetasto=3;  }
      if ((p.y > r1) and (p.y <= r2)) {   codicetasto=codicetasto; }
      if ((p.y > r2) and (p.y <= r3)) {   codicetasto=codicetasto+3;  }
      if ((p.y > r3) and (p.y <= r4)) {   codicetasto=codicetasto+6;  }  
      if (p.y  > r4)                  {   codicetasto=codicetasto+9;  }
      if (codicetasto==11)            {   codicetasto=0;              }                 // controllo 0

      ii=0;

      if (codicetasto == 10)                              // significa premuto tasto C quindi azzero la tastiera e doppio bip
       {    
        ii=3;                                             // il flag ii a 3 significa premuto il tasto c
        for (x=0; x < t_ndigit; x++ ) {  codice [x] = '-';    }         
        Tft.drawString(codice, 15,50,3 , BLUE);                                    
        suona (1);                                   
      }           
      if  (codicetasto == 12) {  ii=5;  }                                      // ii = 5 significa premuto tasto OK
      if (ii < 3)   
      {                                                                        // significa che  un numero da 0 a 9 quindi lo aggiungo a codice e lo stampo                                                                            
        for    ( x=0 ; x < t_ndigit; x++)  {  codice [x] = codice [x+1];  }    // ma prima devo shiftare i numeri immessi precedentemente                                      
        codice[t_ndigit-1]=  ( codicetasto +48);                               // scrivo il codicetasto + 48 per carattere ascii
        Tft.drawString(codice, 15,50,3 , BLUE);                          
      }                                        
    }           // end if presure
  }             // end do  
   
  while (ii < 4 );                                                              //  se ii e uguale a 4 significa che e stato premuto il tasto OK
  // ***************************************************ora  devo convertire il codice in numero 
  if (ii==5)  { 
              x= t_ndigit;
              long moltiplica = 1;
              t_valore=0;
              for (x=t_ndigit-1;   x>=0  ;x--)  
                   { 
                   if (codice[x] == 45) {  codice[x]=48;   }                    // trasformo trattino in zero
                   t_valore = t_valore + ((codice[x]-48) * moltiplica);
                   moltiplica = moltiplica *10;
                   }                       
               }

  if (ii==6)  {  t_valore = -1;   }                                             // timeout  

  Tft.fillScreen ();

}
//  ******************** FINE ROUTINE TASTI *********************************************
//  *************************************************************************************

// **************************************************************************************
//  routine gestione impronte
// **************************************************************************************
void gestione_impronte ()
{
  Tft.fillScreen();                                 // azzero lo schermo
  TFT_BL_ON; 
  x = fps.GetEnrollCount ();
  Tft.drawString("Impr. attive",3,20,2,WHITE);      
  Tft.drawNumber (x,160,15,3,YELLOW);               //  scrivo  le  impronte  attive 

  Tft.drawString("INSERISCO",15,120,3,GREEN);
  Tft.drawString("NUOVA IMPRONTA ? ",20,180,2,WHITE);
  siono();                                          // disegna si o no   
  delay (100); 
  stato_tastiera=5;                                 // stato per gestione impronte 
}

// **************************************************************************************
//  routine per l'inserimento dell'allarme da tastiera
// **************************************************************************************
void inserisci_allarme_da_tastiera ()
{
   wdt_reset();                                   // resetto il watchdog 
   
   RITARDO            = true ;                    // forzo il flag di  ritardo a vero
   ATTENDI_USCITA     = true ;                    // metto il flag di attendi uscita a 1
   ATTENDI_ENTRATA    = false ;                   // resta a false perche non è scattato allarme
   ALLARME_INSERITO   = true ;
   conta_allarmi      = 0;
    
   tempo =millis()/1000;  
   tempo1=tempo;                                  // resetto il timer di tastiera
   tempo_IO=tempo;                                // e anche il timer di IO
   Tft.fillScreen();                              // azzero lo schermo
   stato_tastiera=0; 
   Sstato[0] =1; 
   Schi[0]= (pin+60) ;                            // nel log il pin 
   scrivilog();       // registro su log
}

// **************************************************************************************
//  routine nuovaimpronta fase di inserimento
// ************************************************************************************** 
void nuova_impronta_esegui  ()   
 {
   Tft.fillScreen();   
   TFT_BL_ON;        
   wdt_disable();           // per ora disabilito il watchdog ***********************************
                            // ma inserisco un contatore di un minuto per le impronte 
                            // ******************************************************************
   tempo =millis()/1000;  
   tempo1=tempo;                                     
   int enrollid  = 0;
   bool usedid   = true;
   int risultato;
  
   while (usedid == true)   { usedid = fps.CheckEnrolled(enrollid); if (usedid==true) enrollid++; }  // conto le impronte attive

   fps.EnrollStart(enrollid);
   Tft.drawString ("IL DITO 3 VOLTE",5,100,2,GREEN); 
   for ( int lettura =1;lettura <=3;lettura ++)
         {
         
         fps.SetLED(true);
         
         while(fps.IsPressFinger() == false)                           // aspetto il dito e lampeggio breve
              { 
               fps.SetLED(false);  delay (10);  fps.SetLED(true); 
               tempo =millis()/1000; 
               if (tempo-tempo1 > 100)   { risultato = 1 ; lettura = 4; break ; }    // SE SI SCADUTO IL TEMPO 
              }
                
         fps.CaptureFinger(true);                                       // leggo il dito
         Tft.drawNumber (lettura,lettura * 30,140,2,WHITE);
         switch (lettura) 
              {                                                             
               case 1: fps.Enroll1(); break ;
               case 2: fps.Enroll2(); break ;
               case 3: risultato = fps.Enroll3(); break ;
              }

          for (int i=1; i <= 8; i++) { fps.SetLED(false); delay (100); fps.SetLED(true);}  //lampeggio 8 volte per chiedere il dito 
          
          while(fps.IsPressFinger() == true)   delay(100);               // aspetto che sia tolto il dito
           
          }          // fine del for

   if (risultato== 0) { Tft.drawString ("OK",130,140,2,GREEN); }
        else          { Tft.drawString ("errore",10,160,2,RED);  Tft.drawNumber (risultato,100,160,2,RED);  }

   fps.Close();
   fps.Open();                        // chiudo e riapro il lettore impronte ( non si sa mai)
   fps.SetLED(false);
   wdt_enable(WDTO_8S);               //riabilito il watchdog  *************************
   tempo =millis()/1000;              // riarmo il timer di tastiera
   tempo1=tempo;         
   premiperexit (); 
    
   }
 
//     *********************************************************************************     
//     ************          routine   escludi / includi la sirena 
//     ********************************************************************************* 
void escludisirena ()                                                              
 {
    if (ABILITA_SIRENA) ABILITA_SIRENA=false; 
    else ABILITA_SIRENA = true;

  wdt_reset();  
  stato_tastiera  = 0;
  menu_iniziale ();
  }

//     *************************************************************************************        
//     ************      routine    escludi / includi interni  ovvero microonde e finestre
//     *************************************************************************************        
void escludiinterni ()                                                              
{
     if (ABILITA_INTERNI ) ABILITA_INTERNI=false; 
     else ABILITA_INTERNI = true;

  wdt_reset();  
  stato_tastiera  = 0;  
  menu_iniziale ();
 }

//     ****************************************************************************       
//     **********         routine elenco e gestione dei pin    
//     ****************************************************************************                
void elencodeipin ()           // menu elenco dei pin  
{      
  tempo =millis()/1000;   tempo1=tempo;
  Tft.fillScreen();    
  Tft.drawString (" Elenco dei PIN",6,5,2, BLUE);

  Tft.drawString ("Pin 1", 3,50,2,BLUE);    Tft.drawNumber (pin01,130, 50,2,        YELLOW);
  Tft.drawString ("Pin 2",3,80,2, BLUE);    Tft.drawNumber (pin02,130,80,2,         YELLOW);
  Tft.drawString ("Pin 3",3,110,2,BLUE);    Tft.drawNumber (pin03,130,110,2,        YELLOW);  
  Tft.drawString ("Pin 4 data nasc.",2,140,2, BLUE); 
  
  Tft.drawString ("Vuoi modificarli ?",2,180,2, WHITE);

  siono();                                          // disegna si o no   
  stato_tastiera=6 ;                                // stato per modifica pin 
  
}

//   ****************************************************************************************
//   **                    routine modifica pin
//   ****************************************************************************************
void modificapin ()   // modifica dei pin 
{
  tempo =millis()/1000;   tempo1=tempo;    
  int tasto=0;

  tasti(54,1);  tasto=t_valore;                              // chiedo il numero del pin da modificare
 
  if ((tasto <4) and (tasto >0 )) {  tasti(55,5); }          // chiedo il valore del pin 
     else tasto=5;
   switch (tasto) 
           {   
           case 1:   pin01=t_valore ;  break ;             
           case 2:   pin02=t_valore ;  break ;         
           case 3:   pin03=t_valore ;  break ;       
           default:  Tft.drawString (" errore",3,200,3, RED);    // if nothing else matches, do the default    
           }  
                   
 premiperexit (); 
}

//   ****************************************************************************************
//         routine  impostaora :   imposta l'ora del sistema
//   *****************************************************************************************
void impostaora ()
{  
  ore=0;   
  minuti=0;   
  secondi=0; 

  tasti (10,2);  
  if (t_valore < 0) {   goto fin01;  }   
  ore = t_valore;                                      // digita ora 
  tasti (11, 2); 
  if (t_valore < 0) {  goto fin01;  }   
  minuti=t_valore;                                    // digita minuti  
  tasti (12,2) ; 
  if (t_valore < 0) {  goto fin01;  }   
  secondi=t_valore;                                   // digita secondi

fin01:    
  ttimer=millis()/ 1000;                              // per far ripartire bene i contatori
  ttimer1=ttimer;
  stato_tastiera=0;
  
  premiperexit (); 
  
}  

//   **************************************************************************************
//                  routine cancella impronte   domando si o no 
//   **************************************************************************************
void cancellaimpronte  ()  
{
  Tft.fillScreen();           // azzero lo schermo
  TFT_BL_ON; 
  Tft.drawString("CANCELLO",15,50,3,RED);
  Tft.drawString("TUTTE LE IMPRONTE ",10,100,2,YELLOW);  
  Tft.drawString("SEI SICURA ?",30,170,2,WHITE);
  stato_tastiera=4;                                           // stato per cancella impronte
  siono();                                                    // disegna si o no            

}

//   **************************************************************************************
//          routine menu  cancella_impronte_esegui   si no dopo cancella impronte
//   **************************************************************************************
void cancella_impronte_esegui ()  
{
    Tft.fillScreen();           // azzero lo schermo           
    TFT_BL_ON; 
    fps.DeleteAll ();                        
    Tft.drawString(" CANCELLATE ",3,50,3,YELLOW);         
    delay(100);   
    premiperexit ();
 }


//    ************************************************************************************
//               routine siono  disegna sullo schermo la scelta si o no 
//    ************************************************************************************
void siono ()
{
  Tft.fillRectangle ( 5,260,90,110,             RED);     
  Tft.fillRectangle ( 120,260,100,90,          BLUE);     
  Tft.drawString    ("SI",34,275,3,           WHITE);
  Tft.drawString    ("NO",150,275,3,          WHITE); 
}

//    **************************************************************************************
//    routine leggi_impronta    qualcuno ha premuto tastino del lettore di impronte
//    **************************************************************************************
void leggi_impronta () 
{    
 //  suona (1);     
  Tft.fillScreen();   TFT_BL_ON; 
  strcpy_P(appoggia, Cqualcuno); Tft.drawString(appoggia,50,100,2,RED);      // qualcuno alla porta
  if (ALLARME_INSERITO )  {  lampeggia(20,20); }     else  {  lampeggia(5,300);  }   // lampeggo per segnalare stato allarme

  fps.SetLED(true);
  int identita=0;  
  int dotempoaldito=0;
  delay (50);
  if (dotempoaldito <50)  
  {                                                          // questo loop serve a dare tempo per appoggiare il dito
     identita=300;
     if (fps.IsPressFinger())
        {   
        fps.CaptureFinger(false);                             // false significa lettura a bassa risoluzione
        identita = fps.Identify1_N();                         //   da 0 a 30   200 se sconosciuto aggiungo 1 per lasciare identita zero alla tastiera
        identita=identita+1;          
     
        Tft.drawNumber (identita,50,150,3,WHITE);              //   da 0 a 30   200 se sconosciuto   
           if (identita > 99 ) {  strcpy_P(appoggia, Csconosciuto);Tft.drawString(appoggia,3,200,3,RED); }    //   dito sconosciuto   
        
   //   ********************************************************************************************
   //    controllo se è una identità VERA
   //   ********************************************************************************************
           
           if (identita < 100 )                                                // identita vera !! 
              {                                                            
                 ALLARME_INSERITO = !ALLARME_INSERITO;                         // inverto lo stato di allarme     
                 if (ALLARME_INSERITO ) 
                    { 
                    Tft.drawString(" allarme ON",3,200,3,RED); 
                    lampeggia(20,20); 
                    Sstato[0] =1;
                    }  
               else  
                   { 
                   if (STATO_SIRENA)  { digitalWrite(SIRENA, LOW);  STATO_SIRENA=false; }        // se la sirena è accesa spegnila 
                   Tft.drawString(" allarme OFF",3,200,3,GREEN); 
                   lampeggia(5,300); 
                   Sstato[0] =0; 
                   // ********************  ora azzero anche tutte le condizioni di ritardo 
                      RITARDO          = false;
                      ATTENDI_USCITA   = false;
                      ATTENDI_ENTRATA  = false ;
                      conta_allarmi    = 0;
                      tempo_IO         = 0;
                   }               
               } 
                   
      dotempoaldito=50;                      //   chiudo il conteggio
      
      Schi[0]=identita;    
      scrivilog() ;                          //   aggiornamento log  X 
      }                                      //   end if dito premuto                                        
       
    dotempoaldito=dotempoaldito+1;
    wdt_reset();                             //   azzero il wathdog
    }                                        //   end loop   
            
    fps.SetLED(false);   
    premiperexit ();
}  

//   **************************************************************************************
//    routine consultastorico  stampa  il log degli eventi
//   **************************************************************************************
void consultastorico()                               //  lista dello storico degli accessi
{   
  Tft.fillScreen(); 
  TFT_BL_ON;                                          // azzero lo schermo
  leggiora();                                         //  leggol'ora per aggiustare i giorni 
  Tft.drawString("Chi    hh mm gg st",1,3,2,WHITE);
  int xx=30;
  for ( x=1; x <= 9 ; x++)   
  {  
    if (Sore[x] >=0)                                                // salto le righe con ora a -1 perche vuote             
     {                                                                                                
      if (Schi[x] >= 900 )                                          // è un allarme  
         { 
          Tft.drawString("Allarme",2,xx,2,RED);  
          if (Schi[x] == 991 )   Tft.drawString("Porta",175,xx,2,RED);  // scrivo chi ha provocato l'allarme P M F 
          if (Schi[x] == 992 )   Tft.drawString("Micro",175,xx,2,RED); 
          if (Schi[x] == 993 )   Tft.drawString("Fin",  175,xx,2,RED); 
         }  
        else                                                                   // non è un allarme  
         {                                        
          if (Sstato[x] == 1 ) { Tft.drawString("ON", 180,xx,2,RED  ); }
              else             { Tft.drawString("OFF",180,xx,2,GREEN); }      
         } 

        if (Schi[x] == 201)  Tft.drawString ("???", 5,xx,2,  GREEN);           //  impronta sconosciuta
        
        if (Schi[x] > 59 && Schi[x] < 65)                                      //  pin 
           {
            Tft.drawString ("Pin ",5,xx,2, GREEN);  
            Tft.drawNumber ((Schi[x]-60),55,xx,2,GREEN);
           }
           
        if (Schi[x] > 0 && Schi[x] < 50)                                       // impronta
           {  
            Tft.drawString ("Imp.",5,xx,2, GREEN);  
            Tft.drawNumber (Schi[x],55,xx,2,GREEN); 
           }
                         
            
         Tft.drawNumber (Sore[x],90,xx,2,WHITE);   
         Tft.drawNumber (Sminuti[x],125,xx,2,WHITE);
         Tft.drawNumber ((giorni-Sgiorni[x]),155,xx,2,WHITE);                     
     }
        
    xx=xx+ 22;
   }        

   wdt_reset(); 
   stato_tastiera=0;      
   premiperexit ();
                                            
}

//    *************************************************************************************
//    routine scrivilog sposta verso il basso il lpg eventi e inserisce un nuovo evento 
//    bisogna prima impostare  Schi[0]  e Sstato[0] 
//    *************************************************************************************
void scrivilog ()
{
  leggiora();  
  Sore[0] = ore; 
  Sminuti[0]= minuti; 
  Sgiorni[0] = giorni; 
  int ggx=0; 
  while(ggx  <  9) 
  {  
    x=(8-ggx); 
    Sore[x+1]=Sore[x]; 
    Sminuti[x+1]=Sminuti[x]; 
    Sgiorni[x+1]=Sgiorni[x]; 
    Schi[x+1]=Schi[x]; 
    Sstato[x+1]=Sstato[x]; 
    ggx=ggx+1;
  }
}

//   **************************************************************************************
//    routine premiperexit                                premiperexit
//   **************************************************************************************
 void  premiperexit ()
     { 
     wdt_reset();  
    
     strcpy_P(appoggia, premiperuscire);  Tft.drawString (appoggia,20,300,2, WHITE); 
     tempo =millis()/1000;   tempo1=tempo;   
     stato_tastiera = 0;  
     }

//   ****************************************************************************************
//         routine  leggiora
//   *****************************************************************************************
void leggiora ()
{ 
  ttimer=millis()/ 1000;  
  secondi = secondi + ttimer-ttimer1;
  ttimer1=ttimer;

  for (secondi; secondi >=60; secondi=secondi-60)   {  minuti=minuti+1; }       
  for (minuti; minuti >= 60; minuti= minuti-60)     {  ore=ore+1;       }  
  for (ore; ore >= 24; ore = ore-24)                {  giorni=giorni+1; } 

}

//   **************************************************************************************
//    routine suona  emette un numero di beep passato in ingresso 
//   **************************************************************************************
void suona (int n_volte) 

{  
 do   
  {   
    noTone  (BUZZER);  
    tone    (BUZZER, nota);    
    delay   (100);     
    noTone  (BUZZER);   
    delay   (100);  
    n_volte=n_volte-1;     
    wdt_reset();                                        //resetto il watchdog  
  } 
  while   (n_volte > 1 );     
}
//   **************************************************************************************
//    routine lampeggia emette un numero di lampeggi sul sensore  
//   **************************************************************************************
void lampeggia (int n_volte, int velocita)  
{  
  do   
      {  
       fps.SetLED(true) ; 
       delay(velocita); 
       fps.SetLED(false); 
       delay(velocita);      
       n_volte=n_volte-1;
      } 
  while   (n_volte > 1 );     
}

//   **************************************************************************************
//    routine scattaallarme                   scatta allarme
//                                         con vero = true scatta senza controllare
//   **************************************************************************************
void scattaallarme ()   
  {                          
          
  //       ******************************************************************  
  //        controllo se siamo nello stato  di preallarme ovvero RITARDO = true
  //       lo stato_tastiera di preallarme è quando un allarme è scattato ma si può 
  //       ancora disabilitarlo  **** 
  //       **********************************************************************

  if (! RITARDO) suona_sirena ();                     // se non siamo in ritardo avvio subito la sirena          
        else  
          {
             if  (!ATTENDI_USCITA)                    // se siamo ancora in attesa di uscire non faccio nulla
               {
                  if  (!ATTENDI_ENTRATA)              // se siamo in condizioni di ritardo e il flag di attendi entrata è spento                                                          // significa de devo aspettare l'entrata 
                    {
                    tempo_IO = millis()/1000;         // avvio il timer di attesa allarmi
                    ATTENDI_ENTRATA = true ;          // e avvio il contatore di attesa entrata 
                    TFT_BL_ON;
                    Tft.fillScreen();  
                    stato_tastiera=0;    
                    }
               }   
              TFT_BL_ON;
              Tft.fillScreen();  
              stato_tastiera=0;  
          } 
   } 

//   **************************************************************************************
//    routine conto alla rovescia
//        ATTENDI_USCITA  true significa che è gia trascorsa l'attesa per l'uscita
//        ATTENDI_ENTRATA true significa che è gia trascorsa l'attesa per l'entrata
//
//   **************************************************************************************
void conto_alla_rovescia ()

  {
      if (tempo_IO > 0)                                      // significa che deve iniziare il conteggio
       {    
         wdt_reset();   
         tempo = millis()/1000; 
         x = (temposcaduto_IO-(tempo-tempo_IO));
         if (x != y) 
            {
            TFT_BL_ON;    
            if (ATTENDI_USCITA)  Tft.fillRectangle ( 110, 5,110,80,BLUE); 
               else              Tft.fillRectangle ( 110, 5,110,80,RED);  

            Tft.drawNumber ((temposcaduto_IO-(tempo-tempo_IO)),125,21,5,WHITE);
            y=x;
            
           if (ATTENDI_ENTRATA)                                    // SE STA ENTRANDO EMETTI UN BEEP 
               {
                if (stato_tastiera ==0)    suona(1) ;             // ma non quando digito tasti 
               }
            }
             
//    ******************** adesso controllo se il tempo è scaduto 

         if (tempo-tempo_IO > temposcaduto_IO )                      // SI E' SCADUTO IL TEMPO !!!!
              {
               if (ATTENDI_USCITA)    ATTENDI_USCITA=false;          //  significa che è finita l'uscita
               if (ATTENDI_ENTRATA) 
                  {          
                     ATTENDI_ENTRATA=false; 
                     suona_sirena () ;                               // significa che era scattato un allarme e ho gia atteso l'entrata
                  }            
              }          
     }  

    }

//   **************************************************************************************
//    routine suona_sirena                      accende la sirena e scrive il log per 5 volte
//                                            consecutive poi smette di abilitare la sirena
//   **************************************************************************************
  void suona_sirena()
   {
   if (!STATO_SIRENA)                                //  se la sirena è spenta la riaccendo 
            {  
                                                           
            if (conta_allarmi <  6)                  //  se ho superato 5 allarmi consecutivi smetto di suonare               
               {
               if (ABILITA_SIRENA)   digitalWrite(SIRENA, HIGH);       
               STATO_SIRENA      = true; 
               ALLARME_INSERITO  = true;
               STORIA_ALLARME    = true;
               tempo_SIR         = millis()/1000;             // qui attivazione dello timer di sirena                        
               Sstato[0]         = 1; 
               scrivilog();                                   // aggiorno il registro 
               conta_allarmi ++  ;                            //  incremento il contatore di allarmi    
               Tft.fillScreen();                              // azzero lo schermo;
               TFT_BL_OFF;                                         
               }
           }
    }
    
 //   **************************************************************************************
 //   *********************************   FINE PROGRAMMA  **********************************
 //   **************************************************************************************
