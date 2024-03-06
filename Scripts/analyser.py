import os
import json
from datetime import datetime

def calculate_costs_from_dict(data, electricity_rates, fixed_rate=None):
    total_with_battery = 0
    total_without_battery = 0
    total_fixed_rate = 0
    battery_usage_kwh = 0
    num_entries = len(data['Time'])

    for i in range(num_entries):
        time_str = data['Time'][i]
        if time_str == '24:00':  # Ignore the '24:00' entry
            continue
        hour = datetime.strptime(time_str, '%H:%M').hour

        grid_charge = data['GridCharge'][i] if 'GridCharge' in data else 0
        home_power = data['HomePower'][i] if 'HomePower' in data else 0

        cost_with_battery = (grid_charge * electricity_rates.get(hour, 0)) / 12
        cost_without_battery = (home_power * electricity_rates.get(hour, 0)) / 12
        if fixed_rate is not None:
            cost_fixed_rate = (home_power * fixed_rate) / 12

        battery_usage = max(0, home_power - grid_charge) / 12
        battery_usage_kwh += battery_usage

        total_with_battery += cost_with_battery
        total_without_battery += cost_without_battery
        total_fixed_rate += cost_fixed_rate

    savings = total_without_battery - total_with_battery
    return total_with_battery, total_without_battery, savings, total_fixed_rate, battery_usage_kwh


def calculate_battery_capacity(data):
    full_charge_state = 100  # Assuming 100% is full charge
    min_after_full = 100  # Initialize to full charge
    total_discharge_kwh = 0
    discharge_started = False

    for i in range(len(data['Cbat'])):
        charge_state = data['Cbat'][i]
        grid_charge = data['GridCharge'][i] / 12  # Convert to kWh
        home_power = data['HomePower'][i] / 12  # Convert to kWh

        if charge_state == full_charge_state:
            if discharge_started:
                # Calculate the battery capacity based on the discharge observed
                percentage_discharged = full_charge_state - min_after_full
                if percentage_discharged > 0:
                    estimated_full_capacity_kwh = total_discharge_kwh / percentage_discharged * 100
                    return estimated_full_capacity_kwh
                # Reset for the next discharge cycle
                discharge_started = False
                min_after_full = 100
                total_discharge_kwh = 0
            else:
                discharge_started = True
        elif discharge_started:
            min_after_full = min(min_after_full, charge_state)
            battery_usage = max(0, home_power - grid_charge)
            total_discharge_kwh += battery_usage

    # Handle the case where the file ends before the battery is recharged to 100%
    if discharge_started and min_after_full < full_charge_state:
        percentage_discharged = full_charge_state - min_after_full
        if percentage_discharged > 0:
            estimated_full_capacity_kwh = total_discharge_kwh / percentage_discharged * 100
            return estimated_full_capacity_kwh

    return None



def process_folder(folder_path, electricity_rates, initial_investment, fixed_rate):
    total_price_with_battery = 0
    total_price_without_battery = 0
    total_fixed_rate = 0
    total_savings = 0
    total_battery_usage = 0
    estimated_battery_capacities = []
    json_files = [f for f in os.listdir(folder_path) if f.endswith('.json')]
    json_files.sort()
    
    for file_name in json_files:
        with open(os.path.join(folder_path, file_name), 'r') as file:
            json_data = json.load(file)
            with_battery, without_battery, savings, fixed_cost, battery_usage = calculate_costs_from_dict(json_data['data'], electricity_rates, fixed_rate)
            estimated_capacity = calculate_battery_capacity(json_data['data'])
            if estimated_capacity is not None:
                estimated_battery_capacities.append(estimated_capacity)
            
            print(f"File: {file_name}")
            print(f"Price with Battery: ${with_battery:.2f}")
            print(f"Price without Battery: ${without_battery:.2f}")
            print(f"Price at Fixed Rate: ${fixed_cost:.2f}")
            print(f"Savings: ${savings:.2f}")
            print(f"Battery Usage (kWh): {battery_usage:.2f}")
            '''if estimated_capacity is not None:
                print(f"Estimated Capacity (kWh): {estimated_capacity:.2f}\n")
            else:
                print("Estimated Capacity (kWh): Not available\n")'''

            total_price_with_battery += with_battery
            total_price_without_battery += without_battery
            total_fixed_rate += fixed_cost
            total_savings += savings
            total_battery_usage += battery_usage

    num_days = len(json_files)
    average_daily_savings = total_savings / num_days
    estimated_annual_savings = average_daily_savings * 365
    roi_years = initial_investment / estimated_annual_savings
    average_daily_battery_usage = total_battery_usage / num_days
    average_estimated_capacity = sum(estimated_battery_capacities) / len(estimated_battery_capacities) if estimated_battery_capacities else None

    print("\n\nTotal across all files:")
    print(f"Total Price with Battery: ${total_price_with_battery:.2f}")
    print(f"Total Price without Battery: ${total_price_without_battery:.2f}")
    print(f"Total Price at Fixed Rate ({fixed_rate*100}c / kwh): ${total_fixed_rate:.2f}")
    print(f"Total Savings: ${total_savings:.2f}")
    print(f"Average Daily Savings: ${average_daily_savings:.2f}")
    print(f"Estimated Annual Savings: ${estimated_annual_savings:.2f}")
    print(f"ROI (in years): {roi_years:.2f}")
    print(f"Average Daily Battery Usage (kWh): {average_daily_battery_usage:.2f}")
    #print(f"Average Estimated Battery Capacity: {average_estimated_capacity:.2f}" if average_estimated_capacity is not None else "Battery Capacity Estimation Not Available")

def generate_electricity_rates(rates_list):
    electricity_rates = {}
    for rate_info in rates_list:
        time_range, rate = rate_info
        start_hour, end_hour = map(int, time_range.split('-'))
        for hour in range(start_hour, end_hour + 1):
            electricity_rates[hour] = rate
    return electricity_rates

# Example usage:
rates_list = [["0-5", 0.33], ["6-8", 0.51], ["9-9", 0.33], ["10-15", 0.08], ["16-16", 0.33], ["17-20", 0.75], ["21-23", 0.33]]
electricity_rates = generate_electricity_rates(rates_list)

# This will create a dictionary similar to the one you provided
print(electricity_rates)


folder_path = input("Enter the folder path: ") or "/home/xxxxxxxxxx/Usage"
process_folder(folder_path, electricity_rates, 7650+4200, 0.40)
