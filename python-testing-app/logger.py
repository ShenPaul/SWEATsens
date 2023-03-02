# logger.py
# https://bleak.readthedocs.io/en/latest/api/index.html
# https://medium.com/analytics-vidhya/using-numpy-efficiently-between-processes-1bee17dcb01

# import statements
import sys
import serial
import serial.tools.list_ports
from datetime import datetime
import time
import numpy as np
import matplotlib
import matplotlib.pyplot as plt
from matplotlib.backends.backend_tkagg import (FigureCanvasTkAgg, NavigationToolbar2Tk)
import csv
from tkinter import *
from threading import Thread
from queue import Queue

# from matplotlib.animation import FuncAnimation
# from functools import partial

# plotting in tkinter
matplotlib.use("tkAgg")

# global stop variable
stop = 0

# global queue
time_q = Queue()
val_q = Queue()

# global tkinter window for the buttons
window = Tk()


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


def test_start(btn, ser, val="1"):
    # this function sets the arduino to testing mode
    # the arduino will generate random output data
    # send message to arduino
    ser.write(("0," + val).encode('utf-8'))

    # output and set button colour
    print("Testing mode.")
    btn.config(bg='green')


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


def sens_stop(btn, ser, val="0"):
    # this function stops powering the electrode function
    # send message to arduino
    ser.write(("4," + val).encode('utf-8'))

    # output and reset button colour
    print("Electrode unpowered.")
    btn.config(bg='SystemButtonFace')


def test_stop(btn, ser, val="0"):
    # this function returns the arduino to normal operations
    # send message to arduino
    ser.write(("0," + val).encode('utf-8'))

    # output and reset button colour
    print("Normal mode.")
    btn.config(bg='SystemButtonFace')


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

        # check if there is a comma separator
        # process if there is, pass if there is not
        if ',' in decoded_bytes:

            # convert data a list
            txt = decoded_bytes.split(',')

            # write times to a queue as a float
            global time_q
            time_q.put(float(txt[0]))

            # write voltages to a queue as a float
            global val_q
            val_q.put(float(txt[1]))

            # write the data
            writer.writerow(txt)

    # close the file once the loop is done
    f.close()


def plot():
    # this function creates a new window that plots the data just logged
    # Toplevel object which will be treated as a new window
    plot_window = Toplevel(window)

    # set title
    plot_window.title("Plotting")

    # sets the geometry
    plot_window.geometry("400x400")

    # define a window closing protocol
    plot_window.protocol("WM_DELETE_WINDOW", lambda: on_closing(plot_window))

    # begin plotting
    fig, ax = plt.subplots()

    # create the x and y data from the collected data in the queue
    x_time = np.array(list(time_q.queue))
    y_val = np.array(list(val_q.queue))

    # print(x, y)

    # plot the data
    ax.plot(x_time/1000/60, y_val)

    # the below code is for animations, but it is not necessary
    # line1, = ax.plot([], [], 'ro')
    #
    # def init():
    #     # ax.set_xlim(0, 2 * np.pi)
    #     # ax.set_ylim(-1, 1)
    #     ax.set_xlim(2.3, 2.5)
    #     ax.set_ylim(0, 3.3)
    #     return line1,
    #
    # def update(frame, ln, x, y):
    #     x.append(frame[0]/10000000)
    #     y.append(frame[1])
    #     # x.append(frame)
    #     # y.append(np.sin(frame))
    #     ln.set_data(x, y)
    #     return ln,
    #
    # ani = FuncAnimation(
    #     fig, partial(update, ln=line1, x=[], y=[]),
    #     # frames=np.linspace(0, 2 * np.pi, 128),
    #     frames=zip(x_time, y_val),
    #     init_func=init,
    #     blit=True)

    # this will autoscale the plot to have the correct axes limits
    plt.autoscale()

    # format the plot
    ax.set_xlabel("Time (min)")
    ax.set_xlabel("Voltage (V)")
    ax.set_title("Electrode Voltage")

    fig.set_size_inches(5, 5)

    # create the tkinter canvas for the figure
    canvas = FigureCanvasTkAgg(fig, master=plot_window)
    canvas.draw()

    # place the canvas in the window
    canvas.get_tk_widget().pack()

    # create the toolbar
    toolbar = NavigationToolbar2Tk(canvas, plot_window)
    toolbar.update()

    # place the toolbar in the window
    canvas.get_tk_widget().pack()

    # this will produce a separate plot
    # plt.show()


def on_closing(box):
    # this function defines what happens when the plotting window is closed
    # close the plot so it doesn't run in the background
    plt.close('all')

    # close the window
    box.destroy()


def force_closing(box):
    # stop the program
    box.destroy()
    sys.exit("No Arduino connected!")


def check_presence(correct_port, interval=0.1):
    # this function checks if the arduino is connected and terminated the program if it is diconnected
    # while loop to check
    while True:

        # get the available ports
        myports = [tuple(p) for p in list(serial.tools.list_ports.comports())]

        # if the desired port is not present
        if correct_port not in myports:

            # output
            print("Arduino has been disconnected!")

            # create popup window
            top = Toplevel(window)
            top.geometry("250x50")
            top.title("Error")
            label = Label(top, text="Arduino disconnected!", font=("Times New Roman", 18, "bold"))
            label.pack()

            # set window exit protocol
            top.protocol("WM_DELETE_WINDOW", lambda: force_closing(window))

            break
        # wait 0.1s between checking
        time.sleep(interval)


def main():

    # this starts and gets the list of ports
    myports = [tuple(p) for p in list(serial.tools.list_ports.comports())]

    # see if arduino is connected
    try:
        arduino_port = [port for port in myports if 'COM4' in port][0]

        # start checking thread if it is
        port_controller = Thread(target=check_presence, args=(arduino_port, 0.1,))
        port_controller.setDaemon(True)
        port_controller.start()

    # otherwise raise an error and stop the program
    except:
        # create popup window
        window.geometry("250x50")
        window.title("Error")
        label = Label(window, text="No Arduino connected!", font=("Times New Roman", 18, "bold"))
        label.pack()

        # terminate program on window close
        window.protocol("WM_DELETE_WINDOW", lambda: force_closing(window))

        # start loop
        window.mainloop()

    # define serial port and baud rate
    # find the 'COM#' in the Windows Device Manager
    ser = serial.Serial('COM4', 9600, timeout=1)

    # flush the input
    ser.flushInput()

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

    # plot button
    plot_btn = Button(window, text='Plot', command=plot)
    plot_btn.bind('<Button-1>')

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
    plot_btn.grid(row=2, column=0, sticky="ew")

    # window.geometry("300x200+10+20")
    window.mainloop()


if __name__ == "__main__":
    # main function
    main()
