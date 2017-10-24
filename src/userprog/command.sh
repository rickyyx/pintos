#pintos --filesys-size=2 -p ../../examples/pwd -a pwd -- -f -q run 'pwd'
#pintos --gdb  --filesys-size=2 -p tests/userprog/args-many -a args-many -- -f run args-many
#pintos --gdb  --filesys-size=2 -p tests/userprog/args-many -a args-many -- -f run args-many


# pintos --gdb  --filesys-size=2 -p ../../examples/echo -a echo -- -f run 'echo hello here'

pintos --gdb  --filesys-size=2 -p ../../examples/ls -a '/bin/ls' -- -f run '/bin/ls -l foo bar'
