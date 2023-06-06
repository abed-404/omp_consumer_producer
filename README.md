# omp_consumer_producer
# There is a text file containing the texts. The sentences in this file can be accessed in a random 
# access way. (If there is no such file, create it yourself). A total of 12 threads will run in the system. 
# 8 Threads will be producer threads and 4 threads will be consumer threads. Each consumer thread 
# has a queue structure that holds sentences. 8 producer threads will read the sentences from the 
# file and assign them to the queue structure of the consumer threads. Consumer threads will find 
# the number of characters and words in each sentence. At the end of the program, the number of 
# words and characters in the text file will be written on the screen.
