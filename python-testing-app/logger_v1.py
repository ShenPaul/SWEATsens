# logger.py

# import libraries
import serial
from datetime import datetime
import matplotlib
import matplotlib.pyplot as plt
import numpy as np
import csv
from tkinter import *
from threading import Thread

# plotting in tkinter
matplotlib.use("tkAgg")

# global stop variable
stop = 0


def log_start(btn, ser, val="1"):
    # this function starts logging
    # set a default interval if none is given
    if val is None:
        val = "1"

    # send message to arduino
    ser.write(("3," + val).encode('utf-8'))

    # update stop variable for the new logging thread
    global stop
    stop = 0

    # create new logging thread
    t = Thread(target=logging, args=(ser,))
    t.start()

    # output and set button colour
    print("Logging started.")
    btn.config(bg='green')

    return 1


def stim_start(btn, ser, val="3.3"):
    # this function starts stimulating the skin
    # set a default stimulation voltage if none is given
    if val is None:
        val = "3.3"

    # send message to arduino
    ser.write(("2," + val).encode('utf-8'))

    # output and set button colour
    print("Stimulation started.")
    btn.config(bg='green')
    return 1


def sens_start(btn, ser, val="0.6"):
    # this function starts supplying the electrode with power
    # set a default electrode voltage if none is given
    if val is None:
        val = "0.6"

    # send message to arduino
    ser.write(("4," + val).encode('utf-8'))

    # output and set button colour
    print("Electrode powered.")
    btn.config(bg='green')
    return 1


def test_start(btn, ser, val="1"):
    # this function sets the arduino to testing mode
    # the arduino will generate random output data
    # send message to arduino
    ser.write(("0," + val).encode('utf-8'))

    # output and set button colour
    print("Testing mode.")
    btn.config(bg='green')

    return 1


def log_stop(btn, ser, val="0"):
    # this function stops the logging function
    # send message to arduino
    ser.write(("3," + val).encode('utf-8'))

    # assign global variable and set value to stop
    global stop
    stop = 1

    # output and reset button colour
    print("Logging stopped.")
    btn.config(bg='SystemButtonFace')


def stim_stop(btn, ser, val="0"):
    # this function stops the stimulation function
    # send message to arduino
    ser.write(("2," + val).encode('utf-8'))

    # output and reset button colour
    print("Stimulation stopped.")
    btn.config(bg='SystemButtonFace')
    return 1


def sens_stop(btn, ser, val="0"):
    # this function stops powering the electrode function
    # send message to arduino
    ser.write(("4," + val).encode('utf-8'))

    # output and reset button colour
    print("Electrode unpowered.")
    btn.config(bg='SystemButtonFace')
    return 1


def test_stop(btn, ser, val="0"):
    # this function returns the arduino to normal operations
    # send message to arduino
    ser.write(("0," + val).encode('utf-8'))

    # output and reset button colour
    print("Normal mode.")
    btn.config(bg='SystemButtonFace')
    return 1


def logging(ser):
    # this function writes the data received to a file
    # get the current date for the file name
    now = datetime.now()
    dt_string = now.strftime("%Y-%m-%d-%H-%M-%S")

    # open the file to write to and create the writer
    f = open(dt_string + ".csv", "a", newline='', encoding='utf-8')
    writer = csv.writer(f, delimiter=",")

    # loop while the stop condition is not met
    while stop == 0:

        # read in the next line from serial
        ser_bytes = ser.readline()

        # decode the input and convert it to a string
        decoded_bytes = str(ser_bytes[0:len(ser_bytes) - 2].decode("utf-8"))

        # print data
        print(decoded_bytes)

        # convert data a list
        txt = decoded_bytes.split(',')

        # write the data
        writer.writerow(txt)

    # close the file once the loop is done
    f.close()


def main():
    # define serial port and baud rate
    # find the 'COM#' in the Windows Device Manager
    ser = serial.Serial('COM4', 9600, timeout=1)

    # flush the input
    ser.flushInput()

    # create a tkinter window for the buttons
    window = Tk()

    # create a frame to hold the grid of buttons
    top_frame = Frame(window)

    # logging start button
    log_start_btn = Button(top_frame, text='Start Logging', command=lambda: log_start(log_start_btn, ser, val_entry.get()))
    log_start_btn.bind('<Button-1>')

    # stimulation start button
    stim_start_btn = Button(top_frame, text='Start Stimulation', command=lambda: stim_start(stim_start_btn, ser, val_entry.get()))
    stim_start_btn.bind('<Button-1>')

    # electrode start button
    sens_start_btn = Button(top_frame, text='Power Electrode', command=lambda: sens_start(sens_start_btn, ser, val_entry.get()))
    sens_start_btn.bind('<Button-1>')

    # testing mode button
    test_start_btn = Button(top_frame, text='Testing Mode', command=lambda: test_start(test_start_btn, ser))
    test_start_btn.bind('<Button-1>')

    # logging stop button
    log_stop_btn = Button(top_frame, text='Stop Logging', command=lambda: log_stop(log_start_btn, ser), bg='red')
    log_stop_btn.bind('<Button-1>')

    # stimulation stop button
    stim_stop_btn = Button(top_frame, text='Stop Stimulation', command=lambda: stim_stop(stim_start_btn, ser), bg='red')
    stim_stop_btn.bind('<Button-1>')

    # electrode stop button
    sens_stop_btn = Button(top_frame, text='Stop Electrode', command=lambda: sens_stop(sens_start_btn, ser), bg='red')
    sens_stop_btn.bind('<Button-1>')

    # exit testing mode button
    test_stop_btn = Button(top_frame, text='Normal Mode', command=lambda: test_stop(test_start_btn, ser), bg='red')
    test_stop_btn.bind('<Button-1>')

    # create a field for text entry for potential values to pass to arduino
    bottom_frame = Frame(window)
    val_entry = Entry(bottom_frame)
    val_label = Label(bottom_frame, text="Type values here:")

    # set the window title
    window.title('logger')

    # add the buttons to the frame
    log_start_btn.grid(row=0, column=0, sticky="ew")
    stim_start_btn.grid(row=1, column=0, sticky="ew")
    sens_start_btn.grid(row=2, column=0, sticky="ew")
    test_start_btn.grid(row=3, column=0, sticky="ew")

    log_stop_btn.grid(row=0, column=1, sticky="ew")
    stim_stop_btn.grid(row=1, column=1, sticky="ew")
    sens_stop_btn.grid(row=2, column=1, sticky="ew")
    test_stop_btn.grid(row=3, column=1, sticky="ew")

    # add the entry field and label to the frame
    val_label.grid(row=0, column=0, sticky="ew")
    val_entry.grid(row=0, column=1, sticky="ew")

    # add the frame and the entry field to the window
    top_frame.grid(row=0, column=0)
    bottom_frame.grid(row=1, column=0, sticky="ew")

    # window.geometry("300x200+10+20")
    window.mainloop()


if __name__ == "__main__":
    # main function
    main()
