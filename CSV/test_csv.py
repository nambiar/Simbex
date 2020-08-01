import pandas as pd 
import numpy as np
import csv
import datetime
import time
from time import gmtime, mktime
import matplotlib.pyplot as plt


from datetime import datetime
import pytz

def timestamp_to_utc(time_stamp_string):
    """A function to return time in epoch"""

    time_stamp_string = time_stamp_string.replace("'", "")

    # convert string to utc time
    utc_time = datetime.strptime(time_stamp_string, "%Y-%m-%d %H:%M:%S")

    #convert to epochs
    epoch_time = int((utc_time - datetime(1970, 1, 1)).total_seconds())

    return epoch_time

def get_data_frame():
    """A function to read the csv files and return dataframe objects"""
    roaster = pd.read_csv ("roaster.csv")
    
    activity = pd.read_csv ("activity.csv")
    
    headimpact  = pd.read_csv ("headimpacts.csv")
    
    return roaster, activity,headimpact

def process_data_start_session(roaster, activity, headimpact):
    """A function to process the datafram objects for start and end time"""
    
    print(f"No of players is {len(roaster)}")
    
    min_players = int(len(roaster) * 0.15)
    
    print(f"Min Players for a session req is {min_players}")
    
    #print(activity["PlayerID"][0])
    #activity = activity[:100]
    #headimpact = headimpact[:100]
    
    activity['TimeStamp'] = activity['TimeStamp'].apply(timestamp_to_utc)
    #print(activity.describe())
    headimpact['TimeStamp'] = headimpact['TimeStamp'].apply(timestamp_to_utc)
    
    #session_begin = []
    for timestamps in activity.TimeStamp:
        #print(timestamps)
        activity_within_timerange = activity.loc[(activity['TimeStamp'] >= timestamps)& (activity['TimeStamp'] <= 300 + timestamps)]

        if activity_within_timerange.empty is True:
            continue

        #Check if number of players is 15% of the roaster
        check_for_session_end = False
        if(len(activity_within_timerange) >= min_players):
            print(f'Length is {len(activity_within_timerange)}')
            for row in activity_within_timerange[['PlayerID','TimeStamp']].index:
                #print(activity_within_timestamp['PlayerID'][row])
                players_with_headimpact = headimpact.loc[(headimpact['PlayerID'] == activity_within_timerange['PlayerID'][row]) & (headimpact['TimeStamp'] == activity_within_timerange['TimeStamp'][row])]
                if players_with_headimpact.empty is False:
                    #print(activity_within_timestamp)
                    print(players_with_headimpact)
                    check_for_session_end = True
        
        if check_for_session_end:
            for row in activity_within_timerange[['PlayerID','TimeStamp']].index:
                activity_within_timerange_session_end = activity.loc[(activity['TimeStamp'] > timestamps) & (activity['TimeStamp'] <= 2400 + timestamps) & (activity['PlayerID'] == activity_within_timerange['PlayerID'][row])]


def main():
    roaster, activity, headimpact = get_data_frame()
    process_data_start_session(roaster, activity, headimpact)

if __name__=="__main__":
    main()