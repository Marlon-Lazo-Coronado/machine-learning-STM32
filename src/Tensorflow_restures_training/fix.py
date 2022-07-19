file = open("izquierda_derecha", "r")


out_file = open("izquierda_derecha.csv", "w")

cont = 0

for line in file:
		
	line2 = line.split()
	
	line3 = (",").join(line2)

	out_file.write(line3+"\n")
	
	if (cont == 13000):
		break
	
	cont = cont + 1


file.close()
out_file.close()


























