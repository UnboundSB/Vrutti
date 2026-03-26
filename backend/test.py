import hello

# 1. Instantiate the C++ object, passing the name argument
my_greeter = hello.Greeter("Vrutti Developer")

# 2. Call the public method, which executes the private C++ 'greet' method
result = my_greeter.say_hello()

print(f"C++ Engine says: {result}")