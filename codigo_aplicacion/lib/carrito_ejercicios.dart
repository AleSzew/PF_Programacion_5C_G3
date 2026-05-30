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
    'video': 'https://www.youtube.com/watch?v=MwhnGYwxkS0',
  },
  {
    'id': '3',
    'nombre': 'Sentadilla',
    'video': 'https://www.youtube.com/watch?v=O7pDEpq4n8k',
  },
  {
    'id': '4',
    'nombre': 'Press de banca',
    'video': 'https://www.youtube.com/watch?v=wYXFJZEdZ3A',
  },
  {
    'id': '5',
    'nombre': 'Flexiones de pecho',
    'video': 'https://www.youtube.com/watch?v=5HL5WY0WVJQ',
  },
];

// Carrito: ejercicios que el usuario fue agregando con el botón +.
// Empieza vacío.
List<Map<String, String>> carrito = [];
