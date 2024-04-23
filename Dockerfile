# Use an official Ubuntu base image
FROM ubuntu:latest

# Set environment variables (optional)
ENV DEBIAN_FRONTEND=noninteractive

# Install GCC and essential build tools
RUN apt-get update && apt-get install -y \
    gcc \
    make \
    build-essential \
    && rm -rf /var/lib/apt/lists/*

# Set the working directory (optional)
WORKDIR /usr/src/app

# Copy local C code to the container
COPY . .

# # Compile the C program (replace 'myprogram' with your actual file name without .c)
# RUN gcc -o myprogram myprogram.c

# # Command to run your compiled C program
# CMD ["./myprogram"]
