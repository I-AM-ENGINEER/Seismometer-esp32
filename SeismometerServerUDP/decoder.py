import numpy as np

# Set random seed for reproducibility
np.random.seed(42)

# Simulation Parameters
true_period = 50.0  # True period value
n_points = 500  # Number of measurements
initial_value = 10.0  # Starting value (offset)
noise_std = true_period * 0.05  # 1% noise relative to the period
n_simulations = 5000  # Number of simulation runs

# Probability of a random peak at a measurement and maximum peak amplitude (10% of true_period)
peak_probability = 0.10
max_peak_amplitude = 0.50 * true_period

# Initialize lists to store results for each method
median_periods = []
regression_periods = []
combined_periods = []

for _ in range(n_simulations):
    # Generate measurement indices
    indices = np.arange(n_points)

    # Create the base measurements (a linear signal plus Gaussian noise)
    measurements = initial_value + true_period * indices + np.random.normal(0, noise_std, n_points)

    # Add random peaks: for each measurement, with 10% chance add a random spike
    for i in range(n_points):
        if np.random.rand() < peak_probability:
            # Add a positive spike with amplitude uniformly distributed between 0 and max_peak_amplitude
            measurements[i] += np.random.uniform(0, max_peak_amplitude)

    # Method 1: Consecutive Differences (Median)
    median_periods.append(np.median(np.diff(measurements)))

    # Method 2: Linear Regression (Least Squares Fit)
    slope, _ = np.polyfit(indices, measurements, 1)
    regression_periods.append(slope)

    # Method 3: Combined Method (Linear Regression after Removing Extremes)
    # Sort indices based on measurement values and remove 10 smallest & 10 largest measurements
    sorted_indices = np.argsort(measurements)
    trimmed_indices = sorted_indices[50:-50]

    # Get filtered data for regression
    filtered_indices = indices[trimmed_indices]
    filtered_measurements = measurements[trimmed_indices]

    trimmed_slope, _ = np.polyfit(filtered_indices, filtered_measurements, 1)
    combined_periods.append(trimmed_slope)

# Convert lists to numpy arrays for statistics
median_periods = np.array(median_periods)
regression_periods = np.array(regression_periods)
combined_periods = np.array(combined_periods)

# Compute statistics for all three methods
median_mean = np.mean(median_periods)
median_std = np.std(median_periods)
regression_mean = np.mean(regression_periods)
regression_std = np.std(regression_periods)
combined_mean = np.mean(combined_periods)
combined_std = np.std(combined_periods)

# Output results
print("Results over", n_simulations, "simulations with random peaks added:")
print(f"Method 1 (Median of Differences): Mean = {median_mean:.7f}, Std = {median_std:.7f}")
print(f"Method 2 (Linear Regression):   Mean = {regression_mean:.7f}, Std = {regression_std:.7f}")
print(f"Method 3 (Combined):            Mean = {combined_mean:.7f}, Std = {combined_std:.7f}")
