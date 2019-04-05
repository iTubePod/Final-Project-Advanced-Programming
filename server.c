/*
    Héctor Mauricio González Coello
    A01328258
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
// Signals library
#include <errno.h>
#include <signal.h>
// Sockets libraries
#include <netdb.h>
#include <sys/poll.h>
// Posix threads library
#include <pthread.h>

// Custom libraries
#include "sockets.h"
#include "fatal_error.h"

#define MAX_ACCOUNTS 5
#define BUFFER_SIZE 1024
#define MAX_QUEUE 5

///// Structure definitions

// Data for a single bank account
typedef struct account_struct {
    int id;
    int pin;
    float balance;
} account_t;

// Data for the bank operations
typedef struct bank_struct {
    // Store the total number of operations performed
    int total_transactions;
    // An array of the accounts
    account_t * account_array;
    //Number of accouts
    int total_accounts;
} bank_t;

// Structure for the mutexes to keep the data consistent
typedef struct locks_struct {
    // Mutex for the number of transactions variable
    pthread_mutex_t transactions_mutex;
    // Mutex array for the operations on the accounts
    pthread_mutex_t * account_mutex;
} locks_t;

// Data that will be sent to each structure
typedef struct data_struct {
    // The file descriptor for the socket
    int connection_fd;
    // A pointer to a bank data structure
    bank_t * bank_data;
    // A pointer to a locks structure
    locks_t * data_locks;
} thread_data_t;


///// FUNCTION DECLARATIONS
void usage(char * program);
void setupHandlers();
void initBank(bank_t * bank_data, locks_t * data_locks);
void readBankFile(bank_t * bank_data);
void waitForConnections(int server_fd, bank_t * bank_data, locks_t * data_locks);
void * attentionThread(void * arg);
void closeBank(bank_t * bank_data, locks_t * data_locks);
int checkValidAccount(int account);
/*
    TODO: Add your function declarations here
*/
void onInterruptServer(int signal);
void writeBankFile(bank_t * bank_data);
int getNumberOfTransactions(bank_t* bank_data, pthread_mutex_t* transaction);
float getAccountBalance(thread_data_t* data, int accountNumber);
float accountDeposit(thread_data_t* data, int accountNumber, float amount, int isUniqueTransaction);
float accountWithraw(thread_data_t* data, int accountNumber, float amount, int isUniqueTransaction);
float accountTransfer(thread_data_t* data, int accountFrom, int accountTo, float amount);


///// GLOBAL VARIABLES DECLARATIONS
int interruptFlag = 0;


///// MAIN FUNCTION
int main(int argc, char * argv[])
{
    int server_fd;
    bank_t bank_data;
    locks_t data_locks;

    printf("\n=== SIMPLE BANK SERVER ===\n");

    // Check the correct arguments
    if (argc != 2)
    {
        usage(argv[0]);
    }

    // Configure the handler to catch SIGINT
    setupHandlers();

    // Initialize the data structures
    initBank(&bank_data, &data_locks);

	// Show the IPs assigned to this computer
	printLocalIPs();
    // Start the server
    server_fd = initServer(argv[1], MAX_QUEUE);
	// Listen for connections from the clients
    waitForConnections(server_fd, &bank_data, &data_locks);
    // Close the socket
    close(server_fd);

    // Clean the memory used
    closeBank(&bank_data, &data_locks);

    // Finish the main thread
    pthread_exit(NULL);

    return 0;
}

///// FUNCTION DEFINITIONS

/*
    Explanation to the user of the parameters required to run the program
*/
void usage(char * program)
{
    printf("Usage:\n");
    printf("\t%s {port_number}\n", program);
    exit(EXIT_FAILURE);
}

/*
    Modify the signal handlers for specific events
*/
void setupHandlers()
{
    signal(SIGINT, onInterruptServer); 
}

void onInterruptServer(int signal)
{
    printf("\nYou pressed Ctrl-C\n");
    interruptFlag = 1;
    printf("Finishing the handler\n");
}

/*
    Function to initialize all the information necessary
    This will allocate memory for the accounts, and for the mutexes
*/
void initBank(bank_t * bank_data, locks_t * data_locks)
{
    // Set the number of transactions
    bank_data->total_transactions = 0;

    // Allocate the arrays in the structures
    bank_data->account_array = malloc(MAX_ACCOUNTS * sizeof (account_t));
    // Allocate the arrays for the mutexes
    data_locks->account_mutex = malloc(MAX_ACCOUNTS * sizeof (pthread_mutex_t));

    // Initialize the mutexes, using a different method for dynamically created ones
    //data_locks->transactions_mutex = PTHREAD_MUTEX_INITIALIZER;
    pthread_mutex_init(&data_locks->transactions_mutex, NULL);
    for (int i=0; i<MAX_ACCOUNTS; i++)
    {
        //data_locks->account_mutex[i] = PTHREAD_MUTEX_INITIALIZER;
        pthread_mutex_init(&data_locks->account_mutex[i], NULL);
        // Initialize the account balances too
        bank_data->account_array[i].balance = 0.0;
    }

    // Read the data from the file
    readBankFile(bank_data);
}


/*
    Get the data from the file to initialize the accounts
*/
void readBankFile(bank_t * bank_data)
{
    FILE * file_ptr = NULL;
    char buffer[BUFFER_SIZE];
    int account = 0;
    char * filename = "accounts.txt";

    file_ptr = fopen(filename, "r");
    if (!file_ptr)
    {
        fatalError("ERROR: fopen");
    }

    // Ignore the first line with the headers
    fgets(buffer, BUFFER_SIZE, file_ptr);
    // Read the rest of the account data
    while( fgets(buffer, BUFFER_SIZE, file_ptr) )
    {
        sscanf(buffer, "%d %d %f", &bank_data->account_array[account].id, &bank_data->account_array[account].pin, &bank_data->account_array[account].balance); 
        account++;
    }
    if(account<MAX_ACCOUNTS-1)
    {
        while(account<MAX_ACCOUNTS)
        {
            bank_data->account_array[account].id = account;
            bank_data->account_array[account].pin = 1234;
            bank_data->account_array[account].balance = 0;
            account++;
        }
    }
    bank_data->total_accounts = account;
    
    fclose(file_ptr);
}

void writeBankFile(bank_t * bank_data)
{
    printf("\nSaving session data before exit...\n");
    FILE * file_ptr = NULL;
    int account = 0;
    printf("Found %d accounts to save...", bank_data-> total_accounts);
    char * filename = "accounts.txt";

    file_ptr = fopen(filename, "w");
    if (!file_ptr)
    {
        fatalError("ERROR: fopen");
    }
    fprintf(file_ptr, "Account_number PIN Balance\n");
    // Read the rest of the account data
    while( account < MAX_ACCOUNTS )
    {
        fprintf(file_ptr, "%d %d %f\n", bank_data->account_array[account].id, bank_data->account_array[account].pin, bank_data->account_array[account].balance); 
        account++;
    }
    printf("\nSaving session data before exit...\n");
    fclose(file_ptr);
}

/*
    Main loop to wait for incomming connections
*/
void waitForConnections(int server_fd, bank_t * bank_data, locks_t * data_locks)
{
    struct sockaddr_in client_address;
    socklen_t client_address_size;
    char client_presentation[INET_ADDRSTRLEN];
    int client_fd;
    pthread_t new_tid;
    int poll_response;
    int timeout = 500;

    client_address_size = sizeof client_address;

    while (1)
    {
        struct pollfd pfd[1];
        pfd[0].fd = server_fd;
        pfd[0].events = POLLIN;
        poll_response = poll(pfd, 1, timeout);
        if (poll_response == -1)
        {
            if (errno == EINTR  && interruptFlag)
            {
                printf("Server was interrputed...\n");
            }
            else
            {
                fatalError("ERROR: POLL");
            }
        }
        else if (poll_response == 0)
        {
            if(interruptFlag)
            {
                break;
            }
        }
        else
        {
            if (pfd[0].revents & POLLIN)
            {
                // ACCEPT
                // Wait for a client connection
				client_fd = accept(server_fd, (struct sockaddr *)&client_address, &client_address_size);
				if (client_fd == -1)
				{
					fatalError("ERROR: accept");
				}
				inet_ntop(client_address.sin_family, &client_address.sin_addr, client_presentation, sizeof client_presentation);
				printf("Received incomming connection from %s on port %d\n", client_presentation, client_address.sin_port);
                thread_data_t* connection_data = malloc(sizeof(thread_data_t));
                connection_data->bank_data = bank_data;
                connection_data->data_locks = data_locks;
                connection_data->connection_fd = client_fd;
                int status;
                status = pthread_create(&new_tid, NULL, attentionThread, (void*) connection_data);
                if( status != 0)
                {
                    printf("Failed to create handler!\n");
                }
                else
                {
                    printf("Created thread %d for request.\n", (int)new_tid);
                }

            }
        }
    }
    // Show the number of total transactions
    printf("Processed %i transactions.\n", getNumberOfTransactions(bank_data, &(data_locks->transactions_mutex)));
    // Store any changes in the file
    writeBankFile(bank_data);
}
/*
    Hear the request from the client and send an answer
*/
void * attentionThread(void * arg)
{
    thread_data_t* data = (thread_data_t*) arg;

    struct pollfd pfd[1];
    int timeout = 1;
    int poll_result;
    char buffer[BUFFER_SIZE];
    float transaction = 0;            
    int accountFrom, accountTo;
    operation_t op;
    float value;

    while (interruptFlag==0)
    {
        pfd[0].fd = data->connection_fd;
        pfd[0].events = POLLIN;
        poll_result = poll(pfd, 1, timeout);

        //Poll for client
        if(poll_result!=0)
        {
            //Client disconnected abruptally
            if(recvString(data->connection_fd, buffer, BUFFER_SIZE) == 0)
            {
                printf("Client %d disconnected!\n", data->connection_fd);
                break;
            }

            sscanf(buffer, "%d %d %d %f", (int*)&(op),&accountFrom, &accountTo, &value);

            //Client is disconnecting
            if(op == EXIT)
            {
                printf("Received exit request from client %d\n", data->connection_fd);
                break;
            }

            switch(op)
            {
                // Get balance
                case CHECK:
                    // Validate account
                    if(!checkValidAccount(accountFrom))
                    {
                        sprintf(buffer, "%i %d",  NO_ACCOUNT, 0);
                        sendString(data->connection_fd, buffer, strlen(buffer) + 1);
                        break;
                    }
                    else 
                    {
                        transaction = getAccountBalance(data, accountFrom);
                        sprintf(buffer, "%i %f",  OK, transaction);
                        sendString(data->connection_fd, buffer, strlen(buffer) + 1);
                        break;
                    }
                // Make deposit
                case DEPOSIT:
                    // Validate account
                    if(!checkValidAccount(accountTo))
                    {
                        sprintf(buffer, "%i %d",  NO_ACCOUNT, 0);
                        sendString(data->connection_fd, buffer, strlen(buffer) + 1);
                        break;
                    }
                    else
                    {                    
                        if(value < 0)
                        {
                            sprintf(buffer, "%i %d",  ERROR, 0);
                            sendString(data->connection_fd, buffer, strlen(buffer) + 1);
                            break;
                        }
                        printf("Deposit with value %f\n", value);
                        transaction = accountDeposit(data, accountTo, value, 1);
                        sprintf(buffer, "%i %f",  OK, transaction);
                        sendString(data->connection_fd, buffer, strlen(buffer) + 1);
                        break;
                        
                    }
                // Withdraw money
                case WITHDRAW:
                    // Validate account
                    if(!checkValidAccount(accountFrom))
                    {
                        sprintf(buffer, "%i %d",  NO_ACCOUNT, 0);
                        sendString(data->connection_fd, buffer, strlen(buffer) + 1);
                        break;
                    }
                    else 
                    {
                        if(value >= 0)
                        {
                            transaction = accountWithraw(data, accountFrom, value, 1);
                            if(transaction<0)
                            {
                                sprintf(buffer, "%i %d",  INSUFFICIENT, 0);
                                sendString(data->connection_fd, buffer, strlen(buffer) + 1);
                                break;
                            }
                            sprintf(buffer, "%i %f",  OK, transaction);
                            sendString(data->connection_fd, buffer, strlen(buffer) + 1);
                            break;
                        }
                        else
                        {
                            sprintf(buffer, "%i %d",  ERROR, 0);
                            sendString(data->connection_fd, buffer, strlen(buffer) + 1);
                            break;
                        }
                    }
                // Transfer money between accounts
                case TRANSFER:
                    // Validate accounts
                    if(!checkValidAccount(accountFrom) || !checkValidAccount(accountTo))
                    {
                        sprintf(buffer, "%i %d",  NO_ACCOUNT, 0);
                        sendString(data->connection_fd, buffer, strlen(buffer) + 1);
                        break;
                    }
                    else 
                    {
                        if(value >= 0)
                        {
                            transaction = accountTransfer(data, accountFrom, accountTo, value);
                            if(transaction<0)
                            {
                                sprintf(buffer, "%i %d",  INSUFFICIENT, 0);
                                sendString(data->connection_fd, buffer, strlen(buffer) + 1);
                                break;
                            }
                            sprintf(buffer, "%i %f",  OK, transaction);
                            sendString(data->connection_fd, buffer, strlen(buffer) + 1);
                            break;
                        }
                        else
                        {
                            sprintf(buffer, "%i %d",  ERROR, 0);
                            sendString(data->connection_fd, buffer, strlen(buffer) + 1);
                            break;;
                        }
                    }
                default:
                    fatalError("INVALID OPERATION");
                    break;
            }
        }
    }
    sprintf(buffer, "%i %d",  BYE, 0);
    sendString(data->connection_fd, buffer, strlen(buffer)+1);
    free(data);
    pthread_exit(NULL);
}

/*
    Free all the memory used for the bank data
*/
void closeBank(bank_t * bank_data, locks_t * data_locks)
{
    printf("DEBUG: Clearing the memory for the thread\n");
    free(bank_data->account_array);
    free(data_locks->account_mutex);
}


/*
    Return true if the account provided is within the valid range,
    return false otherwise
*/
int checkValidAccount(int account)
{
    return (account >= 0 && account < MAX_ACCOUNTS);
}

/*
    Returns number of transactions
*/
int getNumberOfTransactions(bank_t* bank_data, pthread_mutex_t* transaction)
{
    int value;
    pthread_mutex_lock(transaction);
    value = bank_data->total_transactions;
    pthread_mutex_unlock(transaction);
    return value;
}

/*
    Returns given account balance
*/
float getAccountBalance(thread_data_t* data, int accountNumber)
{
    account_t* account = &(data->bank_data->account_array[accountNumber]);
    pthread_mutex_t* account_l = &(data->data_locks->account_mutex[accountNumber]);
    pthread_mutex_t* transaction = &(data->data_locks->transactions_mutex);
    float value = -1;

    pthread_mutex_lock(account_l);
    pthread_mutex_lock(transaction);

    value = account->balance;
    printf("%f\n", value);
    data->bank_data->total_transactions++;

    pthread_mutex_unlock(transaction);
    pthread_mutex_unlock(account_l);

    return value;
}

/*
    Makes a deposit to a given account, it it´s a unique transaction, it also adds 1 to the global transaction counter
*/
float accountDeposit(thread_data_t* data, int accountNumber, float amount, int isUniqueTransaction)
{
    account_t* account = &(data->bank_data->account_array[accountNumber]);
    pthread_mutex_t* account_l = &(data->data_locks->account_mutex[accountNumber]);
    pthread_mutex_t* transaction = &(data->data_locks->transactions_mutex);
    float value = -1;


    pthread_mutex_lock(account_l);

    account->balance += amount;
    value = account->balance;

    if(isUniqueTransaction!=0)
        {
            pthread_mutex_lock(transaction);
            data->bank_data->total_transactions++;
            pthread_mutex_unlock(transaction);
        }

    pthread_mutex_unlock(account_l);

    return value;
}

/*
    Makes a withdrawal of money to a given account, it it´s a unique transaction, it also adds 1 to the global transaction counter
*/
float accountWithraw(thread_data_t* data, int accountNumber, float amount, int isUniqueTransaction)
{
    account_t* account = &(data->bank_data->account_array[accountNumber]);
    pthread_mutex_t* account_l = &(data->data_locks->account_mutex[accountNumber]);
    pthread_mutex_t* transaction = &(data->data_locks->transactions_mutex);
    float value = -1;

    pthread_mutex_lock(account_l);

    //insufficient funds;
    if(account->balance < amount)
    {
        value = -1;
    }
    else
    {
        account->balance -= amount;
        value = account->balance;
        if(isUniqueTransaction!=0)
        {
            pthread_mutex_lock(transaction);
            data->bank_data->total_transactions++;
            pthread_mutex_unlock(transaction);
        }
    }

    pthread_mutex_unlock(account_l);
    return value;
}

/*
    Transfers money from one account to another
*/
float accountTransfer(thread_data_t* data, int accountFrom, int accountTo, float amount)
{
    float value = -1;
    float withdrawStatus = accountWithraw(data, accountFrom, amount, 0);
    //if there was enough money in the account and it was successfully withrawed, proceeds with the deposit now
    if(!(withdrawStatus<0))
    {
        pthread_mutex_t* transaction = &(data->data_locks->transactions_mutex);
        value = accountDeposit(data, accountTo, amount, 0);
        pthread_mutex_lock(transaction);
        data->bank_data->total_transactions++;
        pthread_mutex_unlock(transaction);
    }
    return withdrawStatus;
}
