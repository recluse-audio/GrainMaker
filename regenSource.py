import os


# This is for generating a CMake environment variable via the 'set' function
# Use this to quickly regenerate SourceFiles.cmake / TestFiles.cmake

def generate_files_list(root_folders, output_file, variable_name):
    files_list = []
    for root_folder in root_folders:
        for folder, subfolders, files in os.walk(root_folder):
            for filename in files:
                if filename.endswith('.cpp') or filename.endswith('.h'):
                    # Use forward slashes for CMake cross-platform compatibility
                    file_path = os.path.join(folder, filename).replace('\\', '/')
                    files_list.append(file_path)
    files_list.sort()
    files_list = "\n    ".join(files_list)
    output = 'set({}\n    {}\n)'.format(variable_name, files_list)
    with open(output_file, 'w+') as f:
        f.write(output)

source_folders = ['SOURCE', 'SUBMODULES/RD/SOURCE']
generate_files_list(source_folders, 'CMAKE/SOURCES.cmake', 'SOURCES')

test_folders = ['TESTS', 'SUBMODULES/RD/TESTS']
generate_files_list(test_folders, 'CMAKE/TESTS.cmake', 'TESTS')

