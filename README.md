# Spinner
Spinner is a flow clustering algorithm designed to fit existing architectural constraints of SmartNICs, and that can reach line rate for most packet sizes with complexity O(k).

## Usage
After setting up your environment and installing the smartNIC drivers, Spinner can be used following the steps below:

1. Delete the old firmware and configuration files:
> ```
> $ ./clean.sh
> ```
  
2. Compile the Spinner code:
> ```
> $ ./build.sh
> ```

3. Finally, load the firmware onto the smartNIC:
> ```
> $ ./load.sh
> ```

After being loaded onto the SmartNIC, Spinner will automatically initiate the process of identifying and clustering packet flows passing through the board.
