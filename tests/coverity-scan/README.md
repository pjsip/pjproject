# Running Coverity Scan Locally

1. Install Docker
2. Pull and run `ubuntu` image:

   ```
   $ docker pull ubuntu
   $ docker run -it ubuntu
   ```

Run the next steps **inside Docker Ubuntu terminal**

3. Install git:

   ```
   $ sudo apt-update
   $ sudo apt-get install git
   ```
4. Get PJSIP:

   ```
   $ cd
   $ git clone https://github.com/pjsip/pjproject.git
   ```
5. (For now) Switch to `coverity01` branch:

   ```
   $ git checkout coverity01
   ```
6. Get latest version

   ```
   $ git pull
   ```
7. Set Coverity Scan token (from [this page](https://scan.coverity.com/projects/pjsip?tab=project_settings)):

   ```
   $ export COV_TOKEN=....
   ```
8. Run the scan and upload the result (run `run.sh -t` to skip uploading, `-h` to get some help):

   ```
   $ cd pjproject
   $ tests/coverity-scan/run.sh
   ```

