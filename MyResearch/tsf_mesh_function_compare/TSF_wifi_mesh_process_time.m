directory = './data';

files = dir(fullfile(directory, '*_tsf_*.txt'));
files = {files.name};
results = struct();

for i = 1:length(files)
    file_name = files{i};
    data_mesh = dlmread(fullfile(directory, file_name));
    
    start_mesh = data_mesh(:, 1) * 0.025;
    tsf_mesh = data_mesh(:, 2);
    end_mesh = data_mesh(:, 3) * 0.025;
    
    consumed_mesh = end_mesh - start_mesh;
    
    results.(['mesh_', strrep(file_name, '.txt', '')]).start = start_mesh;
    results.(['mesh_', strrep(file_name, '.txt', '')]).tsf = tsf_mesh;
    results.(['mesh_', strrep(file_name, '.txt', '')]).end = end_mesh;
    results.(['mesh_', strrep(file_name, '.txt', '')]).consumed = consumed_mesh;
end

fields = fieldnames(results);
for i = 1:length(fields)
    field = fields{i};
    fprintf('%s mean_time: %f; std: %f\n', field, mean(results.(field).consumed), std(results.(field).consumed));
end
