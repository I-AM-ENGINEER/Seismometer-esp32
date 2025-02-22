results = struct();

file_name = 'capture.txt';
data_mesh = dlmread(file_name);

start_mesh = data_mesh(:, 1);
tsf_mesh = data_mesh(:, 2);

tsf_offset = tsf_mesh(1)
for i = 1:length(tsf_mesh)
    tsf_mesh(i) = tsf_mesh(i) - tsf_offset;
end
% Number of data points
n = length(start_mesh);

% Preallocate array for slopes. The total number of pairs is n*(n-1)/2.
numPairs = n*(n-1)/2;
slopes = zeros(numPairs, 1);
idx = 1;

% Compute slopes for each pair (i, j) where i < j
for i = 1:n-1
    for j = i+1:n
        dx = start_mesh(j) - start_mesh(i);
        if dx ~= 0
            slopes(idx) = (tsf_mesh(j) - tsf_mesh(i)) / dx;
            idx = idx + 1;
        end
    end
end

% Remove any unused preallocated entries (in case of duplicate x-values)
slopes = slopes(1:idx-1);

% Theil-Sen slope is the median of all pairwise slopes
slope = median(slopes);

% The intercept (offset) is computed as the median of (y - slope*x)
offset = median(tsf_mesh - slope .* start_mesh);

% Compute the predicted values
y_estimated = slope .* start_mesh + offset;

% Calculate R² (coefficient of determination)
ss_total = sum((tsf_mesh - mean(tsf_mesh)).^2);
ss_residual = sum((tsf_mesh - y_estimated).^2);
r_squared = 1 - (ss_residual / ss_total);

% Display results
fprintf('Slope: %.15f\n', 1/slope);
fprintf('Offset: %f\n', offset);
fprintf('R² Score: %.15f\n', r_squared);

% Store results in a struct
results.slope = slope;
results.offset = offset;
results.r_squared = r_squared;
