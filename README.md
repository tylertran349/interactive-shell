This is a command-line shell implemented in C++. It supports both interactive and batch modes, making it suitable for both direct command input and scripting.

## Modes

*   **Interactive mode:**  Run commands directly in a command-line interface.
*   **Batch mode:** Execute commands from a specified text file.

## Invocation

*   **Interactive mode (no arguments)**
      
    To start the shell in interactive mode, simply run the executable without any arguments as shown below:
    ```
    prompt> ./shell
    shell>
    ```

    The above starts the shell in interactive mode, where you can type commands directly at the prompt.

*   **Batch mode (one argument - input file name)**
   
    To run the shell in batch mode, provide a text file containing the commands you want to execute as a single command-line argument as shown below:
    ```
    prompt> ./shell batch.txt
    ```

    The above example executes all the commands in batch.txt (a text file with each command on a separate line).
    
    
