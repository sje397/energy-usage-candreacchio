import requests
import json
import time
import os.path
from datetime import datetime, timedelta

authSignature = 'GRABANAUTHSIGNATUREHERE'

def getToken():
	flicker = False
	while not flicker:
		url = "https://monitor.byte-watt.com/api/Account/Login?authsignature="+authSignature+"&authtimestamp=11111"
		payload = json.dumps({
		  "username": "xxxxxxxxxxx@xxxxxxxxxxx.com",
		  "password": "xxxxxxxxxxx"
		})
		headers = {
		  'Content-Type': 'application/json'
		}
		response = requests.request("POST", url, headers=headers, data=payload)
		if len(response.text) > 20:
			flicker = True
			return (json.loads(response.text)['data']['AccessToken'])
		time.sleep(1)




url = "https://monitor.byte-watt.com/api/Power/SticsByDay"

headers = {
  'authtimestamp': '11111',
  'authsignature': authSignature,
  'Content-Type': 'application/json',
  'Authorization': 'Bearer '+getToken()}

targetFolder = "/home/xxxxxxxxxx/Usage/"


# Function to delete existing -temp files
def delete_temp_files(folder):
    for file in os.listdir(folder):
        if file.endswith("-temp.json"):
            os.remove(os.path.join(folder, file))

# Delete existing -temp files before starting the download
delete_temp_files(targetFolder)

# Generate dates from a start date to today (including)
start_date = datetime(2023, 12, 3)
end_date = datetime.now()
date_generated = [start_date + timedelta(days=x) for x in range(0, (end_date-start_date).days + 1)]

for date in date_generated:
    is_today = date.date() == datetime.now().date()
    targetDate = date.strftime("%Y-%m-%d")
    file_suffix = "-temp" if is_today else ""
    file_path = f"{targetFolder}{targetDate}{file_suffix}.json"

    # Check if the file already exists
    if os.path.exists(file_path):
        print(f"File for {targetDate} already exists. Skipping download.")
        continue

    payload = json.dumps({
        "sn": "25000SB235W00029",
        "userId": "25000SB235W00029",
        "szDay": targetDate,
        "isOEM": 0,
        "sDate": "2023-12-03"
    })
    retry = True
    while retry:
        response = requests.request("POST", url, headers=headers, data=payload)
        if "Network exception" not in response.text:
            retry = False
        time.sleep(1)

    with open(file_path, "w") as f:
        f.write(response.text)

    print(response.text)
    time.sleep(1)
