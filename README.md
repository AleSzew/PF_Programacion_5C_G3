 Básicamente, es un asistente para el gimnasio, enfocado en la técnica con el fin de evitar lesiones.
Por ahora se utilizan 3 sensores MPU6050, que miden inclinación y velocidades mientras se realiza el ejercicio. Estos sensores envían los datos por Wi-Fi a un ESP32 principal, que además contiene uno de los tres MPU6050. Este ESP32 analiza los valores provenientes de los tres sensores y, en base a ellos, comprueba si los ejercicios se realizaron correctamente o no.
Hasta acá, eso corresponde a la parte de Arduino, la cual se puede encontrar en el código general y en el código auxiliar (hay un solo código auxiliar, ya que el segundo y tercer módulo son iguales).

Ademas esta la aplicación móvil desarrollada en Flutter, que será utilizada por el usuario.
La aplicación contará con varias funciones, comenzando por un login básico, una pantalla principal (home screen) y un drawer para recorrer las distintas secciones y pantallas de la aplicación. En ella se podrán visualizar datos personales del usuario, así como también sus rutinas y ejercicios.
Tanto las rutinas como los ejercicios serán modificables. Además, antes de realizar cada ejercicio, se podrá ver un video tutorial de YouTube que explique cómo ejecutarlo correctamente.
Al activar el botón de realizar ejercicio, los sensores comenzarán a medir los valores. En base a estos datos, el resultado obtenido por el código de Arduino será enviado por Bluetooth a la aplicación, la cual mostrará dicha información al usuario. Tomar como referencia algunas cosas del diseño la app "Hevy"
