// --- Configuración ESP32 + TB6612FNG ---

// Motor A (Motor izquierdo)
const int PWMA_PIN = 14;
const int AIN1_PIN = 18;
const int AIN2_PIN = 19;

// Motor B (Motor derecho)
const int PWMB_PIN = 15;
const int BIN1_PIN = 16;
const int BIN2_PIN = 17;


// Velocidad inicial PWM (0 - 255)
int VELOCIDAD = 150;

// pines del terminal virtual
const int TX= 0;
const int Rx= 1;

// ---------------- SETUP ----------------

void setup() {

  pinMode(PWMA_PIN, OUTPUT);
  pinMode(AIN1_PIN, OUTPUT);
  pinMode(AIN2_PIN, OUTPUT);

  pinMode(PWMB_PIN, OUTPUT);
  pinMode(BIN1_PIN, OUTPUT);
  pinMode(BIN2_PIN, OUTPUT);
  


  // Terminal virtual Proteus
  // TX = D0(GPIO1)
  // RX = D1(GPIO3)
 
  Serial.begin(9600);


  detener();

  Serial.println("Robot Soccer ESP32 listo");
  Serial.println("Comandos:");
  Serial.println("adelante");
  Serial.println("atras");
  Serial.println("izquierda");
  Serial.println("derecha");
  Serial.println("detener");
  Serial.println("velocidad=0-255");

}



// ---------------- MOVIMIENTOS ----------------


// Adelante
void moverAdelante() {

  // Motor izquierdo
  digitalWrite(AIN1_PIN, LOW);
  digitalWrite(AIN2_PIN, HIGH);


  // Motor derecho
  digitalWrite(BIN1_PIN, HIGH);
  digitalWrite(BIN2_PIN, LOW);


  analogWrite(PWMA_PIN, VELOCIDAD);
  analogWrite(PWMB_PIN, VELOCIDAD);

}



// Atrás
void moverAtras() {


  digitalWrite(AIN1_PIN, HIGH);
  digitalWrite(AIN2_PIN, LOW);


  digitalWrite(BIN1_PIN, LOW);
  digitalWrite(BIN2_PIN, HIGH);


  analogWrite(PWMA_PIN, VELOCIDAD);
  analogWrite(PWMB_PIN, VELOCIDAD);

}



// Giro izquierda
void girarIzquierda() {


  // Motor izquierdo atrás
  digitalWrite(AIN1_PIN, HIGH);
  digitalWrite(AIN2_PIN, LOW);


  // Motor derecho adelante
  digitalWrite(BIN1_PIN, HIGH);
  digitalWrite(BIN2_PIN, LOW);


  analogWrite(PWMA_PIN, VELOCIDAD);
  analogWrite(PWMB_PIN, VELOCIDAD);

}



// Giro derecha
void girarDerecha() {


  // Motor izquierdo adelante
  digitalWrite(AIN1_PIN, LOW);
  digitalWrite(AIN2_PIN, HIGH);


  // Motor derecho atrás
  digitalWrite(BIN1_PIN, LOW);
  digitalWrite(BIN2_PIN, HIGH);


  analogWrite(PWMA_PIN, VELOCIDAD);
  analogWrite(PWMB_PIN, VELOCIDAD);

}



// Detener
void detener() {

  analogWrite(PWMA_PIN,0);
  analogWrite(PWMB_PIN,0);

}



// ---------------- CONTROL SERIAL ----------------


void recibirComando(){


  if(Serial.available()){


    String comando = Serial.readStringUntil('\n');


    comando.trim();


    comando.toLowerCase();



    // Movimiento

    if(comando=="adelante"){

      moverAdelante();
      Serial.println("Movimiento: Adelante");

    }


    else if(comando=="atras"){

      moverAtras();
      Serial.println("Movimiento: Atras");

    }


    else if(comando=="izquierda"){

      girarIzquierda();
      Serial.println("Movimiento: Izquierda");

    }


    else if(comando=="derecha"){

      girarDerecha();
      Serial.println("Movimiento: Derecha");

    }


    else if(comando=="detener"){

      detener();
      Serial.println("Motores detenidos");

    }



    // Control velocidad

    else if(comando.startsWith("velocidad=")){


      String valor = comando.substring(10);


      int nuevaVelocidad = valor.toInt();



      if(nuevaVelocidad >=0 && nuevaVelocidad<=255){


        VELOCIDAD=nuevaVelocidad;


        Serial.print("Velocidad PWM = ");
        Serial.println(VELOCIDAD);

      }

      else{

        Serial.println("Valor fuera de rango 0-255");

      }

    }


    else{

      Serial.println("Comando no reconocido");

    }

  }

}




// ---------------- LOOP ----------------


void loop(){


  recibirComando();


}
