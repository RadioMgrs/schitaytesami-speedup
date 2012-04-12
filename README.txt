Usage:

VideoMarkup videonamefile videomarkupfile outpath outprefix [-options]

videonamefile – full video-filename (including path)
videomarkupfile – markup file. Should contain 4 or 8 lines with 2 numbers each. It is interpreted as one or two quadrangulars outlining the voting boxes.
outpath – ‘/’-ending path to where the output chunks will be written
outprefix – prefix for naming the chunks, e.g. ‘part’ with produce chunks named ‘part_0.avi’,’part_1.avi’ etc.

Options (default):
-erode %d  - a param to clean-up motion mask (2)
-skip %d – skips every %d frames during processing to speed up things (0)
-slow %d – acceleration in the slow parts (2)
-fast %d – acceleration in the fast parts(16)
-sensitivity %f – how sensitive is the background subtraction (0.005)
-minspan %f – how much “padding” in seconds should be added around voting (1)
-meanspan %f – smoothing length in seconds to make acceleration and deceleration gradual (0.5)
-chunk %f – how long in seconds should the chunk be before we start cutting it (12000)
-savesignal 0/1 – save the result of background subtraction to reuse later (1)
-reusesignal 0/1 – reuse the background subtraction from the previous run (0) 
-starttime %d – seconds from midnight for the stopwatch output. If 0 none will be shown (0) 

How to build:
Should build on Windows and latest Ubuntu (following http://mitchtech.net/opencv-ubuntu/)

// Copyright http://schitaytesami.org, 2012
// Author: Victor Lempitsky