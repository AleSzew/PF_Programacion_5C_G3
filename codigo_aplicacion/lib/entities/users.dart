class Users {
  String name;
  String password;
  String email;
  String age;
  String weight;

  Users({
    required this.name,
    required this.password,
    required this.email,
    required this.age,
    required this.weight,
  });
}

Users miUsuario = Users(
  name: 'Ale',
  password: 'Ale',
  email: 'ale@gmail.com',
  age: '20',
  weight: '60',
);
