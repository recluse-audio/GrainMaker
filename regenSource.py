import os


# This is for generating a CMake environment variable via the 'set' function
# Use this to quickly regenerate SourceFiles.cmake / TestFiles.cmake

def generate_files_list(root_folder, output_file, variable_name):
    files_list = []
    for folder, subfolders, files in os.walk(root_folder):
        for filename in files:
            if filename.endswith('.cpp') or filename.endswith('.h'):
                files_list.append(os.path.join(folder, filename))
    files_list.sort()
    files_list = "\n    ".join(files_list)
    output = 'set({}\n    {}\n)'.format(variable_name, files_list)
    with open(output_file, 'w+') as f:
        f.write(output)

generate_files_list('SOURCE', 'CMAKE/SourceFiles.cmake', 'SourceFiles')
generate_files_list('TESTS', 'CMAKE/TestFiles.cmake', 'TestFiles')
