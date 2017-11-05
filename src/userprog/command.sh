#pintos --filesys-size=2 -p ../../examples/pwd -a pwd -- -f -q run 'pwd'
#pintos --gdb  --filesys-size=2 -p tests/userprog/args-many -a args-many -- -f run args-many
#pintos --gdb  --filesys-size=2 -p tests/userprog/args-many -a args-many -- -f run args-many
cd build/ &&
 # make tests/userprog/exit.result VERBOSE=1



#pintos --gdb  --filesys-size=2 -p tests/userprog/exit -a exit -- -q  -f run exit < /dev/null 2> tests/userprog/exit.errors |tee tests/userprog/exit.output
pintos --gdb --filesys-size=2 -p tests/userprog/exit -a exit -- -q -f run exit < /dev/null 2> tests/userprog/exit.errors |tee tests/userprog/exit.output
#pintos --gdb  --filesys-size=2 -p tests/userprog/exit -a exit -- -f -q run exit
# pintos --gdb  --filesys-size=2 -p ../../examples/echo -a echo -- -f run 'echo hello here'

#pintos --gdb  --filesys-size=2 -p ../../examples/ls -a '/bin/ls' -- -f run '/bin/ls -l foo bar'
