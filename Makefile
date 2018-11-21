a3sdn: shared.cpp controller.cpp switch.cpp a3sdn.cpp
	g++ shared.cpp controller.cpp switch.cpp a3sdn.cpp -o a3sdn

clean:
	rm -rf *.o -f; rm -rf .vscode -f; rm fifo-* -f

tar:
	tar -cvf submit.tar ./

end:
	pkill -U $$USER