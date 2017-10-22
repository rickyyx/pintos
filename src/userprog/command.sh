pintos --filesys-size=2 -p ../../examples/pwd -a pwd -- -f -q run 'pwd'


# testing 
pintos -v --gdb  --filesys-size=2 -p tests/userprog/args-none -a args-none -- -q  -f run args-none
