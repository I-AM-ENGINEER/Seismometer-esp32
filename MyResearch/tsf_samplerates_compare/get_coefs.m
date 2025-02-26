Capture_period_ms = 10;
Capture_start_time_s = 0;
Capture_length_s = 3600*10;

% Определение диапазона строк и столбцов
startRow = Capture_start_time_s * (1000/Capture_period_ms); % Первая строка для чтения (с учетом отступа)
endRow = startRow + Capture_length_s * (1000/Capture_period_ms); % Последняя строка для чтения
range = [startRow, 0, endRow, -1]; % Чтение всех столбцов в указанном диапазоне строк


% Чтение данных с учетом табуляции как разделителя
data = dlmread('./data/esp32ap_10ms_24hr.txt', '\t', [startRow 0 endRow 1]);

% Корректировка данных
start_mesh = data(:,1) - data(1,1);
tsf_mesh   = data(:,2) - data(1,2);
n = length(start_mesh);

% Вычисление эталонных значений для всего файла
ref_step = round(1 / 0.0010);  % каждая 1000-я точка
ref_indices = 1:ref_step:n;
[slope_global, offset_global, ~] = computeTheilSen(start_mesh(ref_indices), tsf_mesh(ref_indices), 'Global 10%% dataset');
global_slope_print = 1 / slope_global;

fprintf('Offset: %f\n', offset_global);
fprintf('Slope: %.15f\n', global_slope_print);

% Разбивка данных на чанки по 300 семплов
chunkSize = 300;
numChunks = floor(n / chunkSize);

% Определение процентных долей выборки от 1% до 50% с шагом 1%
samplePercents = 0.04:0.01:0.50;
numPercents = length(samplePercents);

% Предварительное выделение памяти для разностей оценок по каждому чанку
slopeDiff_all  = zeros(numChunks, 1);
offsetDiff_all = zeros(numChunks, numPercents);

for c = 1:numChunks
    fprintf("calc")
    idx_start = (c-1)*chunkSize + 1;
    idx_end = c*chunkSize;
    
    chunk_tim = start_mesh(idx_start:idx_end);
    chunk_tsf   = tsf_mesh(idx_start:idx_end);
    
    [slope, offset, ~] = computeTheilSen(chunk_tim, chunk_tsf, sprintf('Chunk %d - %.2f%%', c, p*100));
    
    ref_point_tim = chunk_tim(ceil(length(chunk_tim)/2));
    ref_point_tsf = getApproxTSF(ref_point_tim, slope, offset);
    glob_point_tsf = getApproxTSF(ref_point_tim, slope_global, offset_global);
    results.chunks(c).timestamp_tim = ref_point_tim;
    results.chunks(c).timestamp_tsf = ref_point_tsf;
    
    slopeDiff_all(c) = glob_point_tsf - ref_point_tsf;
    for k = 1:numPercents
        p = samplePercents(k);
        step = round(1 / p);
        indices = 1:step:chunkSize;
        
        subset_start = chunk_tim(indices);
        subset_tsf   = chunk_tsf(indices);
        
        [slope_sub, offset_sub, ~] = computeTheilSen(subset_start, subset_tsf, sprintf('Chunk %d - %.2f%%', c, p*100));
        sub_slope_print = 1 / slope_sub;
        
        point_tsf = getApproxTSF(ref_point_tim, slope_sub, offset_sub);

        offsetDiff_all(c, k) = abs(point_tsf - ref_point_tsf);
    end
end

avgOffsetDiff = mean(offsetDiff_all, 1);
chunks_tttt = 1:1:numChunks;
chunks_tttt = (chunks_tttt * chunkSize)/100;

% Отфильтровать НЧ

% Parameters
data = slopeDiff_all;
cutoffFreq = 0.1;  % Cutoff frequency (normalized, since period of 5 -> 0.2 cycles/sample)
order = 20;       % Filter order, adjust for better filtering

% Design High-Pass Filter
hpFilter = fir1(order, cutoffFreq, 'high');

% Apply the filter
filteredData = filtfilt(hpFilter, 1, data);

fprintf("STD: %f", std(filteredData));

% Построение графиков усредненных разностей от эталонных значений чанков
figure;

subplot(2,1,1);
hold on;
plot(chunks_tttt, slopeDiff_all, 'b-', 'LineWidth', 1);
%plot(chunks_tttt, filteredData, 'r-', 'LineWidth', 1);
hold off;
xlabel('Device runtime (s)');
ylabel('Offset error (us)');
title('Chunk error from device runtime (us)');
grid on;

subplot(2,1,2);
plot(samplePercents*100, avgOffsetDiff*100, 'r-', 'LineWidth', 1);
xlabel('Sample rate (Hz)');
ylabel('Error from 100SPS (ns)');
title('Разница offset от эталонного значения (чанков)');
grid on;

% Векторизированная функция оценки Theil–Sen
function [slope, offset, r_squared] = computeTheilSen(x, y, label)
    n = length(x);
    pairs = nchoosek(1:n, 2);
    dx = x(pairs(:,2)) - x(pairs(:,1));
    dy = y(pairs(:,2)) - y(pairs(:,1));
    valid = dx ~= 0;
    slopes = dy(valid) ./ dx(valid);
    slope  = median(slopes);
    offset = median(y - slope .* x);
    
    y_est = slope .* x + offset;
    ss_total = sum((y - mean(y)).^2);
    ss_res   = sum((y - y_est).^2);
    r_squared = 1 - (ss_res / ss_total);
end

function tsf_value = getApproxTSF(timestamp, slope, offset)
    tsf_value = slope * timestamp + offset;
end

