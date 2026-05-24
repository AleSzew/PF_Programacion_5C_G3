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
  name: 'ale',
  password: 'ale',
  email: 'ale@gmail.com',
  age: '20',
  weight: '60',
);
