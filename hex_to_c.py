import os

def hex_to_c_header(hex_file_path, output_directory):
    try:
        # Extract the file name (excluding extension) from the path
        file_name, _ = os.path.splitext(os.path.basename(hex_file_path))
        
        # Replace spaces and hyphens with underscores in the file name
        file_name = file_name.replace(" ", "_").replace("-", "_")

        with open(hex_file_path, 'r') as hex_file, open(os.path.join(output_directory, file_name + '.h'), 'w') as header_file:
            # Read lines from the hex file
            lines = hex_file.readlines()

            # Write C header file content
            header_file.write("#ifndef {}_H\n".format(file_name.upper()))
            header_file.write("#define {}_H\n\n".format(file_name.upper()))
            header_file.write("const unsigned char {}[] = {{\n".format(file_name))

            for index, line in enumerate(lines[:-1]):  # Exclude the last line
                # Ignore lines that are not in the expected format
                if not line.startswith(":"):
                    continue

                # Extract data bytes from each line, ignoring the address and last 3 characters
                data_bytes = line[9:-3]

                # Split into pairs of two characters and convert to C array format
                c_array_data = ', '.join(['0x' + data_bytes[i:i+2] for i in range(0, len(data_bytes), 2)])

                # Write C array line to header file
                header_file.write("    " + c_array_data + ("," if index < len(lines) - 2 else "") + "\n")

            # Handle the last line differently
            last_line_data = lines[-1][9:-3]
            c_array_data_last_line = ', '.join(['0x' + last_line_data[i:i+2] for i in range(0, len(last_line_data), 2)])
            header_file.write("    " + c_array_data_last_line + "\n")

            # Complete the C array and close the header file
            header_file.write("};\n\n")
            header_file.write("#endif // {}_H\n".format(file_name.upper()))

        print("Conversion successful. Header file '{}' created.".format(file_name + '.h'))

    except Exception as e:
        print("Error: {}".format(str(e)))

def process_hex_files(directory):
    # Process all hex files in the specified directory
    for filename in os.listdir(directory):
        if filename.endswith(".hex"):
            hex_file_path = os.path.join(directory, filename)
            hex_to_c_header(hex_file_path, directory)


# Example usage:
hex_files_directory = '/home/wai/Arduino/esp32'  # Replace with the path to your directory containing hex files
process_hex_files(hex_files_directory)
