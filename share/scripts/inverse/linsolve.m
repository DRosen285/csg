A = load('$name.gmc');
b = load('$name_noflags.imc');

b(:,2)=b(:,2);
x = b;
x(:,2)=0;

i_start = ($r_min - x(1,1))/(x(2,1) - x(1,1)) + 1;

while(A(i_start,i_start) == 0)
  i_start=i_start+1;
end

% x(i_start:end,2) = linsolve(A(i_start:end,i_start:end),b(i_start:end,2));
x(i_start:end,2) = -2.49*linsolve(A(i_start:end,i_start:end),b(i_start:end,2));

x(1:i_start,2)=x(i_start,2); % that's for the shift of beginning
x(1:end,2)=x(1:end,2)-x(end,2);

save '$name.dpot.matlab' x '-ascii'
quit
