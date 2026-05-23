// Los 5 ejercicios fijos de la app.
// Cada uno es un Map con: id, nombre, video (link de YouTube).
final List<Map<String, String>> listaEjercicios = [
  {
    'id': '1',
    'nombre': 'Curl de bíceps parado',
    'video': 'https://www.youtube.com/shorts/WrpQYs_n_Pw',
  },
  {
    'id': '2',
    'nombre': 'Elevaciones laterales',
    'video': 'https://www.youtube.com/shorts/rv44DZhCO1g',
  },
  {
    'id': '3',
    'nombre': 'Sentadilla',
    'video': 'https://www.youtube.com/shorts/UbIClfnHOuw',
  },
  {
    'id': '4',
    'nombre': 'Press de banca',
    'video': 'https://www.youtube.com/shorts/gRVj1P6a1UE',
  },
  {
    'id': '5',
    'nombre': 'Flexiones de pecho',
    'video': 'https://www.youtube.com/shorts/IODxDxX7oiM',
  },
];

// Carrito: ejercicios que el usuario fue agregando con el botón +.
// Empieza vacío.
List<Map<String, String>> carrito = [];
